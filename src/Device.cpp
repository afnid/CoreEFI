#include "Device.h"

#ifdef ARDUINO

#endif

#ifdef linux

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

static uint32_t perfectus = 0;
static const char *PATH = __FILE__;

uint32_t millis() {
	if (perfectus)
		return micros() / 1000;

	static uint64_t epoch;
	struct timeval tv;
	bzero(&tv, sizeof(tv));

	gettimeofday(&tv, 0);

	uint64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	if (epoch == 0)
		epoch = ms;

	return ms - epoch;
}

uint32_t micros() {
	if (perfectus)
		return ++perfectus;

	static uint64_t epoch;
	struct timeval tv;
	bzero(&tv, sizeof(tv));

	gettimeofday(&tv, 0);

	uint64_t us = tv.tv_sec * 1000000 + tv.tv_usec;

	if (epoch == 0)
		epoch = us;

	return us - epoch;
}

uint32_t ticks() {
	return micros();
}

static int32_t fileLength(const char *path) {
	struct stat st;

	if (::stat(path, &st) < 0)
		return -1;

	return st.st_size;
}
bool saveFile(const char *path, const void *data, uint32_t len) {
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0660);
	bool status = false;

	if (fd > 0) {
		status = write(fd, data, len) == len;

		if (!status)
			myerror();

		close(fd);
	} else
		myerror();

	return status;

}

uint8_t *loadFile(const char *path, int32_t *len) {
	uint32_t length = fileLength(path);

	*len = 0;

	int fd = open(path, O_RDONLY);

	if (fd > 0) {
		void *buf = malloc(length);

		if (read(fd, buf, length) != length)
			myerror();

		close(fd);

		*len = length;

		return (uint8_t *) buf;
	}

	return 0;
}

static uint8_t *flash_init() {
	static uint8_t *flash = 0;

	if (!flash) {
		int32_t len = 0;
		flash = loadFile("main.dat", &len);

		if (!flash || len != 4096) {
			flash = (uint8_t *)calloc(4096, 1);
			saveFile("main.dat", flash, 4096);
		}
	}

	return flash;
}

uint16_t Device::flashSave(void *addr, uint16_t offset, uint16_t length) {
	uint8_t *flash = flash_init();
	memcpy(flash + offset, addr, length);
	saveFile("main.dat", flash, 4096);
	return length;
}

uint16_t Device::flashLoad(void *addr, uint16_t offset, uint16_t length) {
	uint8_t *flash = flash_init();
	memcpy(addr, flash + offset, length);
	return length;
}

void _myerror(const char *path, uint16_t line, ...) {
}

#endif

