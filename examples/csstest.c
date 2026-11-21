#include "../bow/include/Bow.h"

int main(int argc, char **argv) {
	CSSContext css;
	char *b, *b1;
	char *name;
	GList slist;
	CSSSelector cs;
	MemPool mp;

	if (argc < 2)
		exit(1);

	if ((b = qjs_get_file(argv[1])) == NULL)
		exit(1);

	css = CSSParseBuffer(NULL, b, strlen(b), NULL, NULL);
	mp = MPCreate();
	slist = GListCreate();

	cs = CSSCreateSelector(mp);
	CSSSetSelector(cs, "body", NULL, NULL, NULL);
	GListAddHead(slist, cs);

	name = "color";
	if ((b1 = CSSFindProperty(css, slist, name)) != NULL)
		printf("%s == %s\n", name, b1);
	else
		printf("%s not found\n", name);

	CSSPrint(css);
	CSSDestroyContext(css);
	free(b);

	exit(0);
}
