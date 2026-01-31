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

static const char *site_title       = "SEG/FAULT";
static const char *site_link        = "https://h5law.com";
static const char *site_description = "A collection of philosophical essays";

int kevlar_generate_new_rss(const char *folder_path)
{
    Post *posts    = NULL;

    char *filename = malloc(CONFIG_MAX_PATH_SIZE);
    snprintf(filename, CONFIG_MAX_PATH_SIZE, "%s/%s", folder_path, "rss.xml");
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    free(filename);

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        kevlar_err("Unable to open xml file.");
        return 1;
    }

    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    fprintf(fp, "<rss version=\"2.0\" "
                "xmlns:atom=\"http://www.w3.org/2005/Atom\">\n");
    fprintf(fp, "<channel>\n");
    fprintf(fp, "<title>%s</title>\n", site_title);
    fprintf(fp, "<link>%s</link>\n", site_link);
    fprintf(fp, "<description>%s</description>\n", site_description);
    fprintf(fp,
            "<atom:link href=\"%s/rss.xml\" rel=\"self\" "
            "type=\"application/rss+xml\" />\n",
            site_link);

    size_t file_num = kevlar_count_files_in_folder(folder_path, ".html");
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
        if (entry->d_type != DT_REG || !strstr(entry->d_name, ".html"))
            continue;

        if (strcmp(entry->d_name, "index.html") == 0 ||
            strcmp(entry->d_name, "404.html") == 0)
            continue; // 404.html and index.html

        char filepath[CONFIG_MAX_PATH_SIZE];
        snprintf(filepath, CONFIG_MAX_PATH_SIZE, "%s/%s", folder_path,
                 entry->d_name);

        struct stat st;
        if (stat(filepath, &st) != 0)
            continue;

        char         raw_date[64] = "";
        struct statx statxbuf;
        memset(&statxbuf, 0, sizeof(statxbuf));

        if (statx(AT_FDCWD, filepath, AT_SYMLINK_NOFOLLOW, STATX_BTIME,
                  &statxbuf) == 0 &&
            (statxbuf.stx_mask & STATX_BTIME)) {
            time_t     t  = ( time_t )statxbuf.stx_btime.tv_sec;
            struct tm *tm = gmtime(&t);
            strftime(raw_date, sizeof(raw_date), "%Y-%m-%d %H:%M:%S", tm);
        }

        snprintf(posts[num_file].pubDate, sizeof(posts[num_file].pubDate), "%s",
                 raw_date);

        FILE *post_fp = fopen(filepath, "r");
        if (!post_fp)
            continue;

        fseek(post_fp, 0, SEEK_END);
        long fsize = ftell(post_fp);
        fseek(post_fp, 0, SEEK_SET);

        char *buffer = malloc(fsize + 1);
        if (!buffer) {
            fclose(post_fp);
            continue;
        }

        size_t n  = fread(buffer, 1, fsize, post_fp);
        buffer[n] = '\0';
        fclose(post_fp);

        char *title_start = strstr(buffer, "<h2>");
        if (title_start) {
            title_start     += 4;
            char *title_end  = strstr(title_start, "</h2>");
            if (title_end) {
                strncpy(posts[num_file].title, title_start,
                        title_end - title_start);
                posts[num_file].title[title_end - title_start] = '\0';
            } else
                strcpy(posts[num_file].title, "Untitled");
        } else
            strcpy(posts[num_file].title, "Untitled");

        snprintf(posts[num_file].link, sizeof(posts[num_file].link), "%s/%s",
                 site_link, entry->d_name);

        snprintf(posts[num_file].description,
                 sizeof(posts[num_file].description), "An article on %s",
                 posts[num_file].title);

        free(buffer);
        ++num_file;
    }

    closedir(dir);

    for (size_t i = 0; i < num_file; i++) {
        struct tm tm_date;
        memset(&tm_date, 0, sizeof(struct tm));
        if (strptime(posts[i].pubDate, "%Y-%m-%d %H:%M:%S", &tm_date) == NULL) {
            time_t now = time(NULL);
            tm_date    = *gmtime(&now);
        }
        char formatted_pubDate[64];
        strftime(formatted_pubDate, sizeof(formatted_pubDate),
                 "%a, %d %b %Y %H:%M:%S GMT", &tm_date);

        fprintf(fp, "<item>\n");
        fprintf(fp, "<title>%s</title>\n", posts[i].title);
        fprintf(fp, "<link>%s</link>\n", posts[i].link);
        fprintf(fp, "<pubDate>%s</pubDate>\n", formatted_pubDate);
        fprintf(fp, "<description>%s</description>", posts[i].description);
        fprintf(fp, "<guid>%s</guid>\n", posts[i].link);
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
