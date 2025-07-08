module.exports = {
    hooks: {
        readPackage(pkg, ctx) {
            if (pkg.optionalDependencies["llvm-project"] && process.env.BUILD_CLANGD !== "1") {
                delete pkg.optionalDependencies["llvm-project"];
                ctx.log("ignoring LLVM dependency (clangd); to install set BUILD_CLANGD=1 environment variable");
            }
            else if (pkg.optionalDependencies["llvm-project"]) {
                ctx.log("downloading LLVM for clangd build");
            }
            return pkg;
        }
    }
}