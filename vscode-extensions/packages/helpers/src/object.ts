export function deepMerge(...objects: Record<string, any>[]) {
	const isObject = (obj: unknown) => obj && typeof obj === "object";

	return objects.reduce((prev, obj) => {
		for (const [key, vnew] of Object.entries(obj)) {
			const vprev = prev[key];

			if (Array.isArray(vprev) && Array.isArray(vnew)) {
				prev[key] = vprev.concat(...vnew);
			} else if (isObject(vprev) && isObject(vnew)) {
				prev[key] = deepMerge(vprev, vnew);
			} else {
				prev[key] = vnew;
			}
		}

		return prev;
	}, {});
}
