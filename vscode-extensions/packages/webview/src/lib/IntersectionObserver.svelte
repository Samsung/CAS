<script lang="ts">
import { onMount, type Snippet, tick } from "svelte";

interface Props {
	children?: Snippet;
	observed?: Snippet<
		[boolean, IntersectionObserverEntry | null, IntersectionObserver | null]
	>;
	element?: HTMLElement | null;
	once?: boolean;
	intersecting?: boolean;
	root?: HTMLElement | null;
	rootMargin?: string;
	threshold?: number | number[];
	entry?: IntersectionObserverEntry | null;
	observer?: IntersectionObserver | null;
	observe?: (entry: IntersectionObserverEntry) => void;
	intersect?: (entry: IntersectionObserverEntry) => void;
}
let {
	children,
	observed,
	element = null,
	once = false,
	intersecting = $bindable(false),
	root = null,
	rootMargin = "0px",
	threshold = 0,
	entry = null,
	observer = null,
	observe,
	intersect,
}: Props = $props();

let prevRootMargin = $state<string>();
let prevElement = $state<HTMLElement | null>(null);

const initialize = () => {
	observer = new IntersectionObserver(
		(entries) => {
			entries.forEach((_entry) => {
				entry = _entry;
				intersecting = _entry.isIntersecting;
			});
		},
		{ root, rootMargin, threshold },
	);
};
onMount(() => {
	initialize();
	return () => {
		if (observer) {
			observer.disconnect();
			observer = null;
		}
	};
});

$effect(() => {
	if (entry !== null) {
		if (observe) {
			observe(entry);
		}

		if (entry.isIntersecting) {
			if (intersect) {
				intersect(entry);
			}
			if (element && once) {
				observer?.unobserve(element);
			}
		}
	}

	tick().then(() => {
		if (element !== null && element !== prevElement) {
			observer?.observe(element);

			if (prevElement !== null) {
				observer?.unobserve(prevElement);
			}
			prevElement = element;
		}

		if (prevRootMargin && rootMargin !== prevRootMargin) {
			observer?.disconnect();
			prevElement = null;
			initialize();
		}
		prevRootMargin = rootMargin;
	});
});
</script>
{#if children}
	{#key intersecting}
		{@render children()}
	{/key}
{/if}
{#if observed}
	{#key intersecting}
    	{@render observed(intersecting, entry, observer)}
	{/key}
{/if}