sourcen: Number of source files processed for this database
sources: list of source files processed for this database in the format:
	src_file : src_id
directory: Root directory where the processing was performed
typen: Number of types in the database
funcn: Number of functions included in the database
funcdecln: Number of function declarations included in the database
unresolvedfuncn: Number of unresolved functions referenced in the database

globals:
name
hash
id
def
globalrefs
refs:
  typ stalej enumowej uzytej w inicjalizatorze globala
  typ podany do sizeof uzyty w inicjalizatorze globala
  typ podany do offsetof uzyty w inicjalizatorze globala
  bazowy typ dla typu strukturalnego gdy w inicjalizatorze globala jest uzyte wyluskanie
  typ rzutowania (CStyleCast) uzyty w inicjalizatorze globala
funrefs
decls
fid
file
type
linkage
location
deftype
hasinit
init
literals

types:

*x* : x is optional in JSON
$$: this is to be implemented
&x: description of field valid for specific class

empty_record, builtin, pointer, member_pointer, attributed, complex, vector, extended_vector, dependent_sized_array, pack_expansion, unresolved_using, auto, template_type_parm, dependent_name, subst_template_type_parm, record_instantiation, dependent_template_specialization, record_template, record, record_forward, const_array, incomplete_array, variable_array, typedef, enum, enum_forward, decayed_pointer, function, macroqualified

id: unique type id
fid: 
hash
class
qualifiers
size
str
refcount
refs
usedrefs

@member_pointer, dependent_sized_array, pack_expansion, unresolved_using, auto, dependent_name, subst_template_type_parm
dependent

@attributed
attrcore

@record
dependent
location: points to the definition of the record in source code except if the record is an fully specialized template instantiation then it points to the point of instantiation of this record
*outerfn*
*outerfnid*
*cxxrecord*
union
def
*attrrefs* : exists only if there's at least one attribute referenced
*attrnum* : exists only if attrrefs exists
refnames
memberoffsets
globalrefs
bitfields: map that maps { <field_index>: bit width }
decls
*templateref*: reference to template if this is full specialization of some template (written in code) [this is the index to the refs array]
*specialization*: true if this record is a full specialization declaration of some class template (only for cxx records)
*instantiation*: true if this record is an instantiation of some class template (implicit or explicit) (only for cxx records)
*extern*: true if this record is an explicit instantiation declaration of some class template (only if instantiation is true)
*type_args*: type template arguments passed to the template during specialization or instantiation (mandatory if templateref field is present) [those are indexes to the refs array]
*nontype_args*: nontype template arguments passed to the template during specialization or instantiation (mandatory if templateref field is present)
&refs: class references merged with attrrefs (for convenient database query), template reference type and template arguments (assuming this is full specialization of some template)
- first goes list of types for every member of the record (if type is declared as a member of record its index is presented in decls array)
- then goes ids of the types referenced from some record attribute (maximum attrnum ids for referenced attributes); currently only Aligned attribute is checked and we're looking for UnaryExprOrTypeTraitExpr which is ArgumentType as well as for EnumConstant expressions and we'll get the enum type
- finally when this record is full specialization of some class template next goes the id of the main template class and then goes the types for its template type arguments used for this specialization
funrefs
methods: ids of methods in C++ classes
usedef
defhead

@record_forward
dependent
*cxxrecord*
*templateref*
*specialization*
*instantiation*
*type_args*
*nontype_args*
union
def
&refs: There is no class references and attribute references so this field only contains template reference type and template arguments if this is class template specialization forward (e.g. used as a template argument of other template specialization)

@record_template
-qualifiers
location
cxxrecord
union
specialization
*specializations*: list of full specializations for this specific template (only if specialization == false)
*partial_specializations*: list of partial specializations for this specific template (only if specialization == false)
*templateref*: points to the main class template for this specific specialization (only if specialization == true)
def
refnames
bitfields
decls
parms
type_defaults
nontype_defaults
*type_args* : (only if specialization == true)
*nontype_args* : (only if specialization == true)
&refs: 
class references
list of types for every member of the record (if type is declared as a member of record its index is presented in decls array)
Furthermore similar to record additional info is merged with refs, i.e. specializations, partial specializations, templateref, type_args, parms and type_defaults (in that order)
Proper indexes to corresponding values in refs are available in corresponding fields in main record_template structure

$bad location in record forward
$bad location in partial template specialization (1 line lower)

@record_specialization : special type produced to indicate that the type is specialized inside the definition of other template (or partial template)
dependent
cxxrecord
templateref: reference to the template which is specialized
type_args: type template arguments passed to the template during specialization
nontype_args: nontype template arguments passed to the template during specialization
def
$$: location?
$$: type: specialization type at the point of usage (i.e. class template member declaration, function template member declaration, template argument etc. )
&refs: templateref and type_args types which are indexed in corresponding fields

$add references to class template or function template where this specialization was used

@template_type_parm
dependent
-*typeref*
-*funcref*
-*funcdeclref*
-*sugarref*
&refs: depth and index of the template parameter in the enclosing template definition

@subst_template_type_parm
dependent
&refs: first ref describes the type that was placed after the replacement of template type parameter and second ref points to the parameter that was replaced

@dependent_template_specialization
dependent
cxxrecord
-refs
$ write full support

@typedef
name
implicit
def

@enum
*outerfn*
*outerfnid*
values
identifiers
def
dependent

@enum_forward
def

@function
variadic
def


$---- TODO --------------------------------
# other todo?
$fix pack_expansion type (add pattern and number of expansions if present)
$fix dependent_name type to type of nested name specifier into refs and type string
$fix unresolved using
$DependentTemplateSpecialization
$var_template
$type_alias
https://stackoverflow.com/questions/610245/where-and-why-do-i-have-to-put-the-template-and-typename-keywords
--------------------------------------------


funcs:

name: function name
*namespace*: full namespace in which function was defined (empty if global namespace was used)
id
fid
fids
nargs
variadic
firstNonDeclStmt: first non declaration statement in the function code
*inline*
*template*
*classOuterFn*
linkage
*member*
*class*
*classid*
attributes
hash
	templatePars (zawartosc "template_parameters")
	classHash (hash typu w "classid")
	nms (zawartosc "namespace")
	fbody (zawartosc "body")
	ph (referenced static variable name + path to file where it was defined)
cshash: hash of the main function compound statement body "{...}"
*template_parameters*
body: function body (fully preprocessed full function definition)
unpreprocessed_body
declbody: function body until first '{' character
signature: function name + " " + canonical type as string
declhash
	templatePars (zawartosc "template_parameters")
	classHash (hash typu w "classid")
	nms (zawartosc "namespace")
	fdecl_signature (zawartosc "signature")
location
abs_location
start_loc
end_loc
refcount
literals
declcount
taint
calls: list of function ids for all functions called directly in the body of this function
call_info: 
callrefs: arguments passed to each function from the calls list
refcalls: 
refcall_info
refcallrefs: 
switches
csmap
locals
derefs
ifs
asm
globalrefs
globalrefInfo
funrefs
refs
decls
types



funcdecls:

name
*namespace*
id
fid
nargs
variadic
*template*
linkage
*member*
*class*
*classid*
*template_paremeters*
decl: fully printer declaration with removed "extern" keyword from the beginning
signature: function name + " " + canonical type as string
declhash
	templatePars (zawartosc "template_parameters")
	fdecl_signature (zawartosc "signature")
	classHash (hash typu w "classid")
	nms (zawartosc "namespace")
location
refcount
types


unresolvedfuncs:
name
id


missingcallexprn:
missingvardecl:
missingrefsn:
missingtypesn:
missingtypes:
unsupportedfuncclassn:
unsupportedfuncclass:
unsupporteddeclclassn:
unsupporteddeclclass:
unsupportedexprrefsn:
unsupportedexprrefs:
unsupportedattrrefsn:
unsupportedattrrefs:
