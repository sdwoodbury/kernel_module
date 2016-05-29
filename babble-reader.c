#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *driver = "/dev/babbler";
int main(void)
{
	int fd = open(driver, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Could not open %s: %s\n", driver,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	while (1) {
		char buf[1024];
		ssize_t bytes_read = read(fd, buf, sizeof(buf));
		if (bytes_read < 0) {
			fprintf(stderr, "Error reading from %s: %s\n", driver,
				strerror(errno));
			break;
		}
		buf[bytes_read] = '\0';
		printf("%s\n", buf);
	}
	close(fd);
	return 0;
}
