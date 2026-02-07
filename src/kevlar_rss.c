#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "kevlar_handle_config.h"
#include "kevlar_errors.h"
#include "kevlar_build.h"
#include "kevlar_rss.h"
#include "../utils/utils.h"

static KevlarConfig *kevlar_populate_config_struct(KevlarConfig *cfg,
                                                   char         *file_path)
{
    if (!cfg || !file_path)
        return NULL;

    kevlar_get_opt_from_config(file_path, "author", cfg->configAuthor);
    kevlar_get_opt_from_config(file_path, "title", cfg->configTitle);
    kevlar_get_opt_from_config(file_path, "link", cfg->configSiteLink);
    kevlar_get_opt_from_config(file_path, "description",
                               cfg->configSiteDescription);

    return cfg;
}

static size_t str_replace(const char *src, const char *from, const char *to,
                          char *dst, size_t dst_size)
{
    size_t from_len = strlen(from);
    size_t to_len   = strlen(to);
    size_t dst_len  = 0;

    if (from_len == 0)
        return 0;

    while (*src && dst_len < dst_size - 1) {
        if (strncmp(src, from, from_len) == 0) {
            if (dst_len + to_len >= dst_size - 1)
                break;

            memcpy(dst + dst_len, to, to_len);
            dst_len += to_len;
            src     += from_len;
        } else {
            dst[dst_len++] = *src++;
        }
    }

    dst[dst_len] = '\0';
    return dst_len;
}

static int parse_date(const char *str, struct tm *restrict out)
{
    memset(out, 0, sizeof(*out));

    if (strptime(str, "%Y-%m-%d %H:%M:%S", out) == NULL) {
        return -1;
    }

    return 0;
}

int kevlar_generate_new_rss(const char *xml_path, const char *folder_path)
{
    Post *posts                = NULL;
    char  line[MAX_LINE]       = "";
    char  title[128]           = "";
    char  date[128]            = "";
    char  content[MAX_CONTENT] = "";

    char *filename             = malloc(CONFIG_MAX_PATH_SIZE);
    snprintf(filename, CONFIG_MAX_PATH_SIZE, "%s/%s", xml_path, "feed.xml");
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    free(filename);

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        kevlar_err("Unable to open xml file.");
        return 1;
    }

    KevlarConfig cfg = {0};
    kevlar_populate_config_struct(&cfg, "config.ini");

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    fprintf(fp, "<rss version=\"2.0\" "
                "xmlns:atom=\"http://www.w3.org/2005/Atom\">\n");
    fprintf(fp, "<channel>\n");
    fprintf(fp, "<title>%s</title>\n", cfg.configTitle);
    fprintf(fp, "<link>%s</link>\n", cfg.configSiteLink);
    fprintf(fp, "<description>%s</description>\n", cfg.configSiteDescription);
    fprintf(fp,
            "<atom:link href=\"%s/rss.xml\" rel=\"self\" "
            "type=\"application/rss+xml\" />\n",
            cfg.configSiteLink);

    size_t file_num = kevlar_count_files_in_folder(folder_path, "md");
    posts           = calloc(file_num, sizeof(Post));
    if (posts == NULL) {
        kevlar_err("Unable to allocate memory to parse posts.");
        return 1;
    }

    DIR *dir = opendir(folder_path);
    if (!dir) {
        kevlar_err("Unable to open posts directory.");
        return 1;
    }

    int            num_file = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && num_file < file_num) {
        if (!strstr(entry->d_name, "md"))
            continue;
        memset(&posts[num_file], 0, sizeof(Post));

        char filepath[CONFIG_MAX_PATH_SIZE];
        snprintf(filepath, CONFIG_MAX_PATH_SIZE, "%s/%s", folder_path,
                 entry->d_name);

        int fd2   = open(filepath, O_RDONLY, 0644);

        FILE *fp2 = fdopen(fd2, "r");
        if (!fp2) {
            kevlar_err("Unable to open posts file.");
        }

        int in_content = 0;
        while (fgets(line, sizeof(line), fp2)) {
            line[strcspn(line, "\n")] = 0;

            if (!in_content) {
                if (strncmp(line, "Title=", 6) == 0) {
                    strcpy(title, line + 6);
                    strncpy(posts[num_file].title, title,
                            sizeof(posts[num_file].title) - 1);
                    posts[num_file].title[sizeof(posts[num_file].title) - 1] =
                            '\0';

                } else if (strncmp(line, "Date=", 5) == 0) {
                    struct tm tm;
                    if (parse_date(line + 5, &tm) == 0) {
                        strftime(posts[num_file].pubDate,
                                 sizeof(posts[num_file].pubDate),
                                 "%a, %d %b %Y %H:%M:%S GMT", &tm);
                    }
                } else if (line[0] == '#') {
                    in_content = 1;
                }
            } else {
                if (strlen(posts[num_file].content) + strlen(line) + 1 <
                    MAX_CONTENT) {
                    strcat(posts[num_file].content, line);
                    strcat(posts[num_file].content, "\n");
                }
            }
        }
        fclose(fp2);

        snprintf(posts[num_file].link, sizeof(posts[num_file].link), "%s/%s",
                 cfg.configSiteLink, entry->d_name);

        snprintf(posts[num_file].description,
                 sizeof(posts[num_file].description),
                 "Exploring a philosophic topic based on the title '%s'",
                 posts[num_file].title);

        ++num_file;
    }

    closedir(dir);

    for (size_t i = 0; i < num_file; ++i) {
        char link[256] = {0};
        str_replace(posts[i].link, folder_path, xml_path, link, sizeof(link));
        str_replace(posts[i].link, ".md", ".html", link, sizeof(link));

        fprintf(fp, "<item>\n");
        fprintf(fp, "<title>%s</title>\n", posts[i].title);
        fprintf(fp, "<link>%s</link>\n", link);
        fprintf(fp, "<pubDate>%s</pubDate>\n", posts[i].pubDate);
        fprintf(fp, "<description>%s</description>\n", posts[i].description);
        fprintf(fp, "<content>%s</content>\n", posts[i].content);
        fprintf(fp, "<guid>%s</guid>\n", link);
        fprintf(fp, "</item>\n");
    }

    free(posts);

    fprintf(fp, "</channel>\n");
    fprintf(fp, "</rss>\n");
    fclose(fp);

    kevlar_ok("RSS feed generated: rss.xml\n");
    return 0;
}

/* vim: ts=4 sts=4 sw=4 cin ai et */
