#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    char title[256];
    char link[256];
    char pubDate[64];  // RFC 822 format, e.g., "Wed, 30 Jan 2026 12:00:00 GMT"
} Post;

static const char *site_title = "SEG/FAULT";
static const char *site_link = "https://h5law.com";
static const char *site_description = "A collection of philosophical essays";

int kevlar_generate_new_rss(const char *folder_path)
{
    Posts *posts = NULL;

    FILE *fd = fopen(snprintf("%s/%s", folder_path, "feed.xml", CONFIG_MAX_PATH_SIZE), "w");
    if (!fd) {
        kevlar_err("Unable to open xml file");
        return 1;
    }

    fprintf(fd, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    fprintf(fd, "<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n");
    fprintf(fd, "<channel>\n");
    fprintf(fd, "<title>%s</title>\n", site_title);
    fprintf(fd, "<link>%s</link>\n", site_link);
    fprintf(fd, "<atom:link href=\"%s/feed.xml\" rel=\"self\" type=\"application/rss+xml\" />\n", site_link);

    size_t fileNum = kevlar_count_files_in_folder(folder_path, ".html");
    posts = malloc(sizeof(Posts) * fileNum);
    if (posts == NULL) {
        kevlar_err("Unable to allocate memory to parse posts.");
        return 1;
    }

    DIR *dir = opendir(folder_path);
    if (!dir) {
        kevlar_err("Unable to open posts directory.");
        return 1;
    }

struct dirent *entry;
while ((entry = readdir(dir))) {
    if (entry->d_type != DT_REG || !strstr(entry->d_name, ".html")) continue;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, entry->d_name);

    struct stat st;
    if (stat(filepath, &st) != 0) continue;

    char raw_date[64] = "";
    struct statx statxbuf;
    memset(&statxbuf, 0, sizeof(statxbuf));
    if (statx(AT_FDCWD, filepath, AT_SYMLINK_NOFOLLOW, STATX_BTIME, &statxbuf) == 0 &&
        (statxbuf.stx_mask & STATX_BTIME)) {
        struct tm *tm = gmtime(&statxbuf.stx_btime.tv_sec);
        strftime(raw_date, sizeof(raw_date), "%Y-%m-%d %H:%M:%S", tm);
    } else {
        struct tm *tm = gmtime(&st.st_ctime);
        strftime(raw_date, sizeof(raw_date), "%Y-%m-%d %H:%M:%S", tm);
    }

    strcpy(posts[num_posts].pubDate, raw_date);

    FILE *post_fp = fopen(filepath, "r");
    if (!post_fp) continue;

    fseek(post_fp, 0, SEEK_END);
    long fsize = ftell(post_fp);
    fseek(post_fp, 0, SEEK_SET);

    char *buffer = malloc(fsize + 1);
    if (!buffer) {
        fclose(post_fp);
        continue;
    }
    fread(buffer, 1, fsize, post_fp);
    buffer[fsize] = '\0';
    fclose(post_fp);

    char *title_start = strstr(buffer, "<h2>");
    if (title_start) {
        title_start += 4
        char *title_end = strstr(title_start, "</h2>");
        if (title_end) {
            strncpy(posts[num_posts].title, title_start, title_end - title_start);
            posts[num_posts].title[title_end - title_start] = '\0';
        } else {
            strcpy(posts[num_posts].title, "Untitled");
        }
    } else {
        strcpy(posts[num_posts].title, "Untitled");
    }

    snprintf(posts[num_posts].link, sizeof(posts[num_posts].link), "%s", site_link, entry->d_name);

    free(buffer);
    num_posts++;
}

    closedir(dir);

    for (int i = 0; i < num_posts; i++) {
        struct tm tm_date;
        memset(&tm_date, 0, sizeof(struct tm));
        if (strptime(posts[i].pubDate, "%Y-%m-%d %H:%M:%S", &tm_date) == NULL) {
            time_t now = time(NULL);
            tm_date = *gmtime(&now);
        }
        char formatted_pubDate[64];
        strftime(formatted_pubDate, sizeof(formatted_pubDate), "%a, %d %b %Y %H:%M:%S GMT", &tm_date);

        fprintf(fp, "<item>\n");
        fprintf(fp, "<title>%s</title>\n", posts[i].title);
        fprintf(fp, "<link>%s</link>\n", posts[i].link);
        fprintf(fp, "<pubDate>%s</pubDate>\n", formatted_pubDate);
        fprintf(fp, "<guid>%s</guid>\n", posts[i].link);
        fprintf(fp, "</item>\n");
    }

    free(posts);

    fprintf(fd, "</channel>\n");
    fprintf(fd, "</rss>\n");
    fclose(fd);

    kevlar_ok("RSS feed generated: feed.xml\n");
    return 0;
}

/* vim: ts=4 sts=4 sw=4 cin ai et */
