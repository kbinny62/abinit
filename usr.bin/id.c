/**
 * POSIX system utility: id
 *  - Outputs effective IDs, unless -r (real IDs) is specified
 *  - Outputs numeric IDs, unless -n (string IDs) is specified
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

struct {
	char *exename;
	int opt_G, opt_g;
	int opt_n, opt_r, opt_u;
	int opt_a; /* compat */
} _g;

void usage(const char *msg)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", _g.exename, msg);
	fprintf(stderr, "Usage: %s [-Ggu]\n", _g.exename);
	exit(EXIT_FAILURE);
}

static const char *uid_to_str(uid_t uid)
{
	struct passwd *pwd;
	if ((pwd = getpwuid(uid)) != NULL)
		return pwd->pw_name;
	else
		return NULL;
}

static const char *gid_to_str(gid_t gid)
{
	struct group *grp;
	if ((grp = getgrgid(gid)) != NULL)
		return grp->gr_name;
	else
		return NULL;
}

int main(int argc, char *argv[])
{
	int opt;

	_g.exename = argv[0];
	while ((opt = getopt(argc, argv, "aGgnru")) != -1) {
		switch (opt) {
		case 'G':
			if (_g.opt_g || _g.opt_u)
				usage("Options [Ggu] are mutually exclusive");
			_g.opt_G = 1; break;
		case 'g':
			if (_g.opt_G || _g.opt_u)
				usage("Options [Ggu] are mutually exclusive");
			_g.opt_g = 1; break;
		case 'n':
			_g.opt_n = 1; break;
		case 'r':
			_g.opt_r = 1; break;
		case 'u':
			if (_g.opt_G || _g.opt_g)
				usage("Options [Ggu] are mutually exclusive");
			_g.opt_u = 1; break;
		case 'a':
			break;
		default:
			usage(NULL);
		}
	}

	if (_g.opt_u) {
		uid_t uid = _g.opt_r ? getuid() : geteuid();
		if (_g.opt_n)
			fprintf(stdout, "%s\n", uid_to_str(uid));
		else
			fprintf(stdout, "%u\n", uid);
	} else if (_g.opt_g) {
		gid_t gid = _g.opt_r ? getgid() : getegid();
		if (_g.opt_n)
			fprintf(stdout, "%s\n", gid_to_str(gid));
		else
			fprintf(stdout, "%u\n", gid);
	} else if (_g.opt_G) {
		gid_t rgid=getuid(), egid=getegid(), *groups;
		ssize_t ngroups, ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;
		groups = (gid_t *)malloc(ngroups_max * sizeof(gid_t));
		ngroups = getgroups(ngroups_max, groups);
		fprintf(stdout, _g.opt_n ? "%s %s " : "%u %u ",
			_g.opt_n ? gid_to_str(rgid) : rgid,
			_g.opt_n ? gid_to_str(egid) : egid);
		for (ngroups_max=0; ngroups_max<ngroups; ngroups_max++)
			fprintf( stdout, _g.opt_n ? "%s " : "%u ",
				_g.opt_n ? gid_to_str(groups[ngroups_max]) : \
						groups[ngroups_max]);
		putchar('\n');
					
	} else {
		uid_t uid = getuid(), euid = geteuid();
		gid_t gid = getgid(), egid = getgid();
		gid_t *groups;

		ssize_t ngroups, ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1, i;
		groups = (gid_t *)malloc(ngroups_max * sizeof(gid_t));
		ngroups = getgroups(ngroups_max, groups);

		fprintf(stdout, "uid=%u", uid);
		if (uid_to_str(uid))
			fprintf(stdout, "(%s)", uid_to_str(uid));
		fprintf(stdout, " gid=%u", gid);
		if (gid_to_str(gid))
			fprintf(stdout, "(%s)", gid_to_str(gid));
		if (euid != uid) {
			fprintf(stdout, " euid=%u", euid);
			if (uid_to_str(euid))
				fprintf(stdout, "(%s)", uid_to_str(euid));
		}
		if (egid != gid) {
			fprintf(stdout, " egid=%u", egid);
			if (gid_to_str(egid))
				fprintf(stdout, "(%s)", gid_to_str(egid));
		}
		if (ngroups > 0)
			for (i=0; i<ngroups; i++) {
				const char *gid_str = gid_to_str(groups[i]);
				fprintf(stdout, i ? ",%u" : " groups=%u", groups[i]);
				if (gid_str)
					fprintf(stdout, "(%s)", gid_str);
			}
		putchar('\n');
	}

	return EXIT_SUCCESS;
}

