#include "../bow/include/Bow.h"

int main(int argc, char **argv) {
	char *html_input;
	bool is_test = false;

	if (argc < 2 || (argc > 1 && (html_input = qjs_get_file(argv[1])) == NULL)) {
		is_test = true;
		html_input =
			"<!DOCTYPE html>"
			"<html lang=\"en\">"
			"<head>"
			"  <meta charset=\"UTF-8\">"
			"  <title>Caf&eacute; &amp; Bistro &#x2615;</title>"
			"  <script>const check = (a < b && b > 0) ? '&ok' : 'no';</script>"
			"</head>"
			"<body>"
			"  <main id=\"app\">"
			"    <p id=\"old-msg\">Temporary loading message...</p>"
			"    <p id=\"remove-me\">Obsolete banner to delete</p>"
			"    <video controls><source src=\"intro.mp4\" type=\"video/mp4\"></video>"
			"  </main>"
			"  <script type=\"module\">console.log('Second script loaded & ready');</script>"
			"</body>"
			"</html>";
	}

	/* 1. Parse HTML5 document */
	HtmlDom_t *doc = html_parse(html_input);
	printf("=== 1. Initial Parsed DOM Tree ===\n");
	dom_print(doc, 0);

	/* 2. Extract raw <script> contents (preserving '<', '>', '&' unescaped) */
	size_t i, script_count = 0;
	char **scripts = dom_extract_tag_contents(doc, TAG_SCRIPT, &script_count);
	printf("=== Extracted %zu <script> Block(s) ===\n", script_count);
	for (i = 0; i < script_count; i++) {
		printf("[Script %zu]: %s\n", i + 1, scripts[i]);
	}
	html_string_array_free(scripts, script_count);

	if (is_test) {
		/* 3. Mutate DOM using dom_replace_child and dom_remove_child */
		HtmlNodeList *p_nodes = dom_get_elements_by_tag_id(doc, TAG_P);
		HtmlDom_t *old_msg = NULL;
		HtmlDom_t *remove_me = NULL;

		for (i = 0; i < p_nodes->count; i++) {
			const char *id = dom_get_attr(p_nodes->items[i], "id");
			if (id && strcmp(id, "old-msg") == 0) old_msg = p_nodes->items[i];
			if (id && strcmp(id, "remove-me") == 0) remove_me = p_nodes->items[i];
		}
		dom_node_list_free(p_nodes);

		if (old_msg && dom_parent(old_msg)) {
			HtmlDom_t *new_article = dom_create_element("article");
			dom_set_attr(new_article, "class", "welcome-card");
			HtmlDom_t *h1 = dom_create_element("h1");
			dom_append_child(h1, dom_create_text("Welcome to Café & Bistro ☕"));
			dom_append_child(new_article, h1);

			HtmlDom_t *detached = dom_replace_child(dom_parent(old_msg), new_article, old_msg);
			dom_free(detached);
		}

		if (remove_me && dom_parent(remove_me)) {
			HtmlDom_t *detached = dom_remove_child(dom_parent(remove_me), remove_me);
			dom_free(detached);
		}

		/* 4. Serialize mutated DOM back to pretty HTML5 */
		char *serialized = dom_serialize(doc, true);
		printf("\n=== Serialized Mutated HTML5 ===\n%s", serialized);
		free(serialized);
	}

	dom_free(doc);
	doc = NULL;
	const char *sample_html =
		"<!DOCTYPE html>"
		"<html lang=\"en\">"
		"<head>"
		"  <meta charset=\"UTF-8\">"
		"  <title>News &amp; Updates &#8212; Demo</title>"
		"  <style>body > p { color: #333; } /* <not-a-tag> */</style>"
		"</head>"
		"<body>"
		"  <header id=\"top-header\">"
		"    <nav><a class=\"nav-link active\" href=\"/home\">Home</a></nav>"
		"  </header>"
		"  <main>"
		"    <article class=\"featured\" data-category=\"tech\">"
		"      <h1>HTML5 &amp; C Parser</h1>"
		"      <p id=\"intro\">Price: &euro;49 &lt; &pound;50 &copy; 2026</p>"
		"      <textarea name=\"bio\">User &lt;bio&gt; with <b>raw</b> text &amp; entities</textarea>"
		"      <input type=\"text\" placeholder=\"Enter &quot;name&quot;\">"
		"      <custom-widget data-id=\"42\">Custom Element</custom-widget>"
		"    </article>"
		"  </main>"
		"  <script>"
		"    const msg = '<div>This is RAWTEXT, not a DOM node</div>';"
		"    if (1 < 2 && 3 > 1) { console.log(msg); }"
		"  </script>"
		"</body>"
		"</html>";

	doc = html_parse(sample_html);
	HtmlDom_t *article = dom_query_selector(doc, "article.featured");
	HtmlDom_t *widget = dom_query_selector(doc, "custom-widget");
	HtmlDom_t *intro = dom_query_selector(doc, "#intro");
	HtmlDom_t *new_footer;
	HtmlDom_t *removed;

	printf("\n=== 1. Initial Parsed DOM Tree ===\n");
	dom_print(doc, 0);

	/* Demonstrate DOM Mutation Helpers */
	if (intro) {
		dom_set_attr(intro, "class", "highlight");
		dom_set_attr(intro, "data-updated", "true");
		dom_set_text_content(intro, "Updated intro text via dom_set_text_content()");
	}

	if (article && widget) {
	  /* Detach custom-widget and free it */
		removed = dom_remove_child(article, widget);
		dom_free(removed);

		/* Create and append a new <footer> element */
		new_footer = dom_create_element("footer");
		dom_set_attr(new_footer, "id", "article-footer");
		dom_set_text_content(new_footer, "Added dynamically with dom_create_element_by_name()");
		dom_append_child(article, new_footer);
		dom_print(article, 0);
	}

	printf("\n=== 2. Mutated DOM Tree ===\n");
	dom_print(doc, 0);
	dom_free(doc);

	return 0;
}
