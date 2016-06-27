#include <cstdio>
#include <cstdlib>
#include <queue>
#include <set>
#include <git2.h>

static void
giterr_fatal(const char* msg)
{
    fprintf(stderr, "%s: %s\n", msg, giterr_last()->message);
    exit(EXIT_FAILURE);
}

struct revwalk_queue_entry {
    git_commit* commit;

    revwalk_queue_entry(git_commit* commit) : commit(commit) {}

    git_time_t committed_at() const {
        return git_commit_committer(commit)->when.time;
    }

    bool operator<(const revwalk_queue_entry& other) const {
        return committed_at() < other.committed_at();
    }
};

struct comparable_git_oid {
    git_oid oid;

    comparable_git_oid(const git_oid* oid) : oid(*oid) {}

    bool operator<(const comparable_git_oid& other) const {
        return memcmp(&oid, &other.oid, sizeof(oid)) < 0;
    }
};

struct revwalk {
    git_repository* repo;

    std::priority_queue<revwalk_queue_entry> pq;

    std::set<comparable_git_oid> seen;

    git_oid cursor_root;
    size_t cursor_offset;

    revwalk(git_repository* repo, git_commit* start_commit)
        : repo(repo)
    {
        pq.push(revwalk_queue_entry(start_commit));

        cursor_root = *git_commit_id(start_commit);
        cursor_offset = 0;
    }

    bool has_next() {
        return !pq.empty();
    }

    git_oid next_oid() {
        revwalk_queue_entry ent = pq.top();

        pq.pop();

        unsigned int parent_count = git_commit_parentcount(ent.commit);

        if (parent_count == 1 && pq.empty()) {
            cursor_root = *git_commit_parent_id(ent.commit, 0);
            cursor_offset = 0;
        } else {
            cursor_offset++;
        }

        for (unsigned int i = 0; i < parent_count; i++) {
            const git_oid* parent_oid = git_commit_parent_id(ent.commit, i);

            if (seen.find(comparable_git_oid(parent_oid)) != seen.end()) {
                continue;
            }

            git_commit* parent;

            if (git_commit_lookup(&parent, repo, parent_oid)) {
                giterr_fatal("git_commit_lookup");
            }

            pq.push(revwalk_queue_entry(parent));

            seen.insert(comparable_git_oid(parent_oid));
        }

        git_oid oid = *git_commit_id(ent.commit);

        git_commit_free(ent.commit);

        return oid;
    }
};

static git_commit*
revparse_commit(git_repository* repo, const char* ref)
{
    git_object* object;

    if (git_revparse_single(&object, repo, ref)) {
        giterr_fatal("git_revparse_single");
    }

    git_commit* commit;

    if (git_commit_lookup(&commit, repo, git_object_id(object))) {
        giterr_fatal("git_commit_lookup");
    }

    git_object_free(object);

    return commit;
}

int
main(int argc, const char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: git-cursor-log <ref-ish> [<skip>]\n");
        exit(EXIT_FAILURE);
    }

    const char* ref = argv[1];

    size_t skip = 0;

    if (argc > 2) {
        skip = atoi(argv[2]);
    }

    const char* git_dir = getenv("GIT_DIR");

    if (!git_dir) {
        fprintf(stderr, "must set GIT_DIR\n");
        exit(EXIT_FAILURE);
    }

    git_libgit2_init();

    git_repository* repo;

    if (git_repository_open(&repo, git_dir)) {
        giterr_fatal("git_repository_open");
    }

    revwalk rw(repo, revparse_commit(repo, ref));

    for (size_t i = 0; i < skip && rw.has_next(); i++) {
        rw.next_oid();
    }

    while (rw.has_next()) {
        // read cursor before advancing in next_oid call:
        char cursor_root_str[41];
        git_oid_tostr(cursor_root_str, sizeof(cursor_root_str), &rw.cursor_root);

        size_t cursor_offset = rw.cursor_offset;

        git_oid oid = rw.next_oid();

        char oid_str[41];
        git_oid_tostr(oid_str, sizeof(oid_str), &oid);

        printf("%s+%zu  %s\n", cursor_root_str, cursor_offset, oid_str);
    }

    return 0;
}
