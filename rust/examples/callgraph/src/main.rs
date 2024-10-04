use clap::Parser;
use ftdb::{FunctionEntry, FunctionId};
use ftdb_ext::callgraph::Callgraph;
use std::fs::File;
use std::io::Write;

struct DotGraph {
    edges: Vec<String>,
    config_preamble: String,
}

impl DotGraph {
    fn new() -> DotGraph {
        DotGraph {
            edges: Vec::new(),
            config_preamble: String::from(
                "\tfontname=\"Helvetica,Arial,sans-serif\"\n\
                \tnode [fontname=\"Helvetica,Arial,sans-serif\"]\n\
                \tedge [fontname=\"Helvetica,Arial,sans-serif\"]\n\
                \tnode [shape=box];\n\n",
            ),
        }
    }

    fn add_edge(&mut self, source: &str, target: &str) {
        let edge = format!("\t\"{}\" -> \"{}\"", source, target);
        self.edges.push(edge);
    }

    fn get_dot(&self) -> String {
        let edge_entries = self.edges.join("\n");
        let graph_program = format!("digraph {{\n{}{}\n}}\n", self.config_preamble, edge_entries);
        graph_program
    }
}

/// A simple tool for unfolding graphs and saving them to .dot files (or printing to stdout).
#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    /// the filename of the FTDB image
    #[arg(short, long)]
    image_file: String,

    /// the name of the root function for unfolding
    #[arg(short, long)]
    root_func_name: String,

    /// flag true if function pointers should be resolved; defaults to false
    #[arg(long, default_value_t = false)]
    resolve_fptrs: bool,

    /// the depth of the unfolding
    #[arg(short, long)]
    unfolding_depth: u8,

    /// the name of the output dot file; defaults to None (i.e. stdout)
    #[arg(short, long)]
    dot_output_file: Option<String>,

    /// flag true if reverse call graph should be used; defaults to false
    #[arg(long, default_value_t = false)]
    reverse_calls: bool,
}

fn main() -> Result<(), String> {
    let args = Args::parse();

    let db = ftdb::load(args.image_file);
    let db = match db {
        Ok(database) => database,
        Err(ftdberr) => return Err(ftdberr.to_string()),
    };

    if args.resolve_fptrs {
        return Err(String::from(
            "Function pointer inference not implemented yet.",
        ));
    }

    let mut matching_funcs = db.funcs_by_name(&args.root_func_name).peekable();
    if matching_funcs.peek().is_none() {
        return Err(format!("function {0} not found.", &args.root_func_name,));
    }
    let matching_func_ids = matching_funcs.map(|entry| entry.id());

    // choose call or reverse call graph iterator, depending on request
    let callgraph_iterator = if args.reverse_calls {
        Box::new(db.iter_up(matching_func_ids, args.unfolding_depth))
            as Box<dyn std::iter::Iterator<Item = (FunctionId, FunctionEntry<'_>, u8)>>
    } else {
        Box::new(
            db.iter_down(matching_func_ids)
                .with_depth(args.unfolding_depth)
                .into_iter(),
        )
    };

    // printing callgraph
    let mut dot_program = DotGraph::new();

    for entry in callgraph_iterator {
        let source = db
            .func_by_id(entry.0)
            .expect("'None' source in callgraph iterator.");
        let target = entry.1;

        if args.dot_output_file.is_none() {
            // print to stdout
            let direction = if args.reverse_calls {
                "<--(is called by)--"
            } else {
                "--(calls)-->"
            };

            println!(
                "{} ({}) {} {} ({})",
                source.name(),
                source.id(),
                direction,
                target.name(),
                target.id()
            );
        } else {
            // construct dot object
            dot_program.add_edge(source.name(), target.name());
        }
    }

    // save to file
    if let Some(output_file) = args.dot_output_file {
        let dotfile = File::create(output_file.clone());
        match dotfile {
            Ok(mut file) => {
                let _ = write!(file, "{}", dot_program.get_dot());
                println!(
                    "Done. Saved .dot program to {output_file}. \
                    Run \"dot -Tpdf {output_file} -o {output_file}.pdf\" to produce pdf file."
                );
            }
            Err(err) => return Err(err.to_string()),
        };
    }

    Ok(())
}
