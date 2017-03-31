#include "ClientOSX.h"

#include "os-detection.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <sstream>
#include "v8.h"

#define LOG_TAG "ClientOSX"

const char* ClientOSX::loadFile (const char* path) {

	struct stat fileStat;
	if (stat(path, &fileStat)) {
		LOGE("Cannot find file %s", path);
		return NULL;
	}
	int fd = open (path, O_RDONLY);

	const int count = fileStat.st_size;
	char *buf = (char*)malloc(count+1), *ptr = buf;
	bzero(buf, count+1);
	int bytes_read = 0, bytes_to_read = count;
	std::stringstream str;

	while ((bytes_read = read(fd, buf, bytes_to_read)) > 0) {
		bytes_to_read -= bytes_read;
		ptr += bytes_read;
		str << "Read " << bytes_read << " bytes from total of " << count << ", " << bytes_to_read << " left";
		LOGI(str.str().c_str());
	}
	close(fd);
	return buf;
}

ClientAbstract::~ClientAbstract() { }

ClientOSX::~ClientOSX () {

}
