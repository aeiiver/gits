// compile: gcc -ggdb -o scmv scmv.c -lgit2
// usage:   [NO_COLOR=1] ./scmv

#include "c.h"
#include <unistd.h>
#include <git2.h>

typedef struct {
    char      *path;
    buf_maybe  summary;
    buf_maybe  body;

    buf        author;
    buf        author_email;
    long       author_time;
    int        author_datestamp;

    buf        committer;
    buf        committer_email;
    long       committer_time;
    int        committer_datestamp;
} contrib;

static arr(contrib, 1 << 16) contribs;

static int compare_contribs(const void *a, const void *b)
{
    const contrib *ah = a, *bh = b;
    return ah->committer_time - bh->committer_time;
}

static int find_index_nearest_contrib_by_time(int datestamp)
{
    int low = 0, high = contribs.len;
    int mid = (high + low) / 2;
    while (low < high) {
        if        (datestamp > contribs.arr[mid].committer_datestamp) {
            low = mid + 1;
            mid = (high + low) / 2;
        } else if (datestamp < contribs.arr[mid].committer_datestamp) {
            high = mid - 1;
            mid = (high + low) / 2;
        } else {
            return mid;
        }
    }
    return 0;
}

static char *contrib_color(int contrib_count)
{
    char *no_color = getenv("NO_COLOR");
    if (no_color && no_color[0] != 0) switch (contrib_count) {
        case 0:  return "0";
        case 1:  return "1";
        case 2:  return "2";
        case 3:  return "3";
        case 4:  return "4";
        default: return "*";
    } else switch (contrib_count) {
        case 0:  return "\033[90m"      " " "\033[0m";
        case 1:  return "\033[1;97;46m" " " "\033[0m";
        case 2:  return "\033[1;97;42m" " " "\033[0m";
        case 3:  return "\033[1;97;43m" " " "\033[0m";
        case 4:  return "\033[1;97;41m" " " "\033[0m";
        default: return "\033[1;97;45m" " " "\033[0m";
    }
}

static inline int packed_datestamp(int year, int month, int day) { return year*10000 + month*100 + day*1; }
static inline bool is_leap_year(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

int main(int argc, char *argv[])
{
    char *prog_name = shift(&argv);
    if (prog_name == 0) {
        eprintln("error: program was invoked with an empty argument list");
        exit(1);
    }
    if (argv[0] == 0) {
        eprintln("usage: %s dirs...", prog_name);
        exit(1);
    }

    if (git_libgit2_init() < 0) {
        eprintln("error: libgit2: %s", git_error_last()->message);
        exit(1);
    }
    for (char *path; (path = shift(&argv));) {
        git_repository *repo;
        int err = git_repository_open(&repo, path);
        if (err < 0) {
            if (err == GIT_ENOTFOUND) {
                eprintln("warn: %s", git_error_last()->message);
                continue;
            }
            eprintln("error: %s", git_error_last()->message);
            exit(1);
        }

        git_revwalk *revwalk;
        if (git_revwalk_new(&revwalk, repo) < 0) {
            eprintln("error: %s", git_error_last()->message);
            exit(1);
        }
        if (git_revwalk_push_head(revwalk)) {
            eprintln("warn: %s", git_error_last()->message);
            continue;
        }

        for (git_oid oid; git_revwalk_next(&oid, revwalk) == 0;) {
            git_commit *commit;
            if (git_commit_lookup(&commit, repo, &oid) < 0) {
                eprintln("error: %s", git_error_last()->message);
                exit(1);
            }

            const char *summary = git_commit_summary(commit);
            const char *body = git_commit_body(commit);

            const git_signature *author_sig = git_commit_author(commit);
            struct tm *author_sig_time = gmtime(&author_sig->when.time);
            int author_datestamp = packed_datestamp(author_sig_time->tm_year + 1900, author_sig_time->tm_mon + 1, author_sig_time->tm_mday);

            const git_signature *committer_sig = git_commit_committer(commit);
            struct tm *committer_sig_time = gmtime(&committer_sig->when.time);
            int committer_datestamp = packed_datestamp(committer_sig_time->tm_year + 1900, committer_sig_time->tm_mon + 1, committer_sig_time->tm_mday);;

            arr_push(&contribs, ((contrib){
                .path                = path,
                .summary             = copy_str_maybe(summary),
                .body                = copy_str_maybe(body),

                .author              = copy_str(author_sig->name),
                .author_email        = copy_str(author_sig->email),
                .author_time         = author_sig->when.time,
                .author_datestamp    = author_datestamp,

                .committer           = copy_str(author_sig->name),
                .committer_email     = copy_str(author_sig->email),
                .committer_time      = author_sig->when.time,
                .committer_datestamp = committer_datestamp,
            }));

            git_commit_free(commit);
        }

        git_revwalk_free(revwalk);
        git_repository_free(repo);
    }
    qsort(&contribs.arr, contribs.len, sizeof(contribs.arr[0]), compare_contribs);

    int year = 2024;

    struct tm this_year = {.tm_year = year - 1900, .tm_mon = 0, .tm_mday = 1};
    mktime(&this_year);

    printf("DEBUG ROW:\n");
    for (int i = 0; i < 8; i += 1) printf("0123456789");
    printf("\n");

    static char display_buf[1+7][54];
    static char *display_text = display_buf[0];
    static char (*display_grid)[arrcapof(display_buf[0])] = &display_buf[1];
    struct { char name; int days; } month_days[] = {
        {'J', 31},
        {'F', is_leap_year(this_year.tm_year + 1900) ? 29 : 28},
        {'M', 31},
        {'A', 30},
        {'M', 31},
        {'J', 30},
        {'J', 31},
        {'A', 31},
        {'S', 30},
        {'O', 31},
        {'N', 30},
        {'D', 31},
    };
    int week_day = this_year.tm_wday;
    int week_nr = 0;

    for (int m = 0; m < arrcapof(month_days); m += 1) {
        display_text[week_nr] = month_days[m].name;
        for (int d = 0; d < month_days[m].days; d += 1) {
            int contrib_count = 0;
            int datestamp = packed_datestamp(year, m + 1, d + 1);
            int index = find_index_nearest_contrib_by_time(datestamp);
            if (index > 0) {
                while (contribs.arr[index].committer_datestamp == datestamp) {
                    contrib_count += 1;
                    index += 1;
                }
            }
            display_grid[week_day++][week_nr] = contrib_count;
            if (week_day == 7) {
                week_day = 0;
                week_nr += 1;
                debug_assert(week_nr < arrcapof(display_grid[0]), "grid is not large enough");
            }
        }
    }

    printf("%c", display_buf[0][0] ?: ' ');
    for (int x = 1; x < arrcapof(display_buf[0]); x += 1)
        printf(" %c", display_buf[0][x] ?: ' ');
    printf("\n");

    for (int y = 1; y < arrcapof(display_buf); y += 1) {
        printf("%s", contrib_color(display_buf[y][0]));
        for (int x = 1; x < arrcapof(display_buf[0]); x += 1)
            printf(" %s", contrib_color(display_buf[y][x]));
        printf("\n");
    }
}
