#define MAX_LINE 80
#define MAX_CONTENT 16384

typedef struct {
    char title[256];
    char link[256];
    char description[512];
    char pubDate[64]; // RFC 822 format, e.g., "Wed, 30 Jan 2026 12:00:00 GMT"
    char content[MAX_CONTENT];
} Post;

int kevlar_generate_new_rss(const char *xml_path, const char *folder_path);

/* vim: ts=4 sts=4 sw=4 cin ai et */
