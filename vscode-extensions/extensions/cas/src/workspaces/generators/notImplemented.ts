import { DepsGenerator } from "./generator";

const NotImplementedGenerator: DepsGenerator = {
	createDeps() {
		throw new Error("Not implemented");
	},
	update() {
		throw new Error("Not implemented");
	},
};
export default NotImplementedGenerator;
