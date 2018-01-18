/*
Вариант 4:

    /
    `--bar/755
        `--baz/744
            |--bin/177
            |--readme.txt/544
            |--example/555
            `--foo/711
               |--cp/444
               `--test.txt/777

Дополнительно должна быть реализована функция `rename`.

gcc task5.c -o task5 -lfuse -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=31
./task5 /home/nastya27/5/22
tree

fusermount -u 22
*/

#define FUSE_USE_VERSION 31

#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int _counter = 0; //число всех папок и файлов в системе

typedef struct File { //структура описывающая сущность 
	char *path; //ее расположение
	int rights; //права доступа
	bool isDirectory;  //папка или файл
	char *contents;   //содержание сущности 
} File;

#define MAX_FILES 1000
File files[MAX_FILES];

int is_counter(int i) {
	return i == _counter ? -ENOENT : 0;
}

bool is_slash(const char *str, int from) { //если путь к файлу вложенный, то переходим на следующий уровень
	for (int i = from; i < strlen(str); i++) {
		if (str[i] == '/') {
			return true;
		}
	}
	return false;
}

static int q_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	int i;
	for (i = 0; i < _counter; i++) {
		if (!strcmp(path, files[i].path)) {
			int flag = files[i].isDirectory ? S_IFDIR : S_IFREG;
			stbuf->st_mode = flag | files[i].rights;
			stbuf->st_nlink = files[i].isDirectory ? 2 : 1;
			if (!files[i].isDirectory) {
				stbuf->st_size = strlen(files[i].contents);
			}
			break;
		}
	}
	return is_counter(i);
}

static int q_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi) //чтение пути файла, выводит все файлы в директории и "подгружает" права каждой из них
{
	(void)offset;
	(void)fi;
	int i;
	for (i = 0; i < _counter; i++) {
		printf("%s \n",files[i].path);
		int len_i = strlen(files[i].path);
		if (files[i].isDirectory && !strcmp(path, files[i].path)) {
			if (files[i].rights < 0400) {
				return -EACCES;
			}
			filler(buf, ".", NULL, 0); //заполнение директории нашими файлами
			filler(buf, "..", NULL, 0);
			for (int j = 0; j < _counter; j++) {
				int len_j = strlen(files[j].path);
				if (len_i < len_j && !strncmp(files[j].path, files[i].path, len_i)) {
					if (!strcmp(files[i].path, "/") && !is_slash(files[j].path, len_i)) {
						filler(buf, files[j].path + len_i, NULL, 0);
					}
					else if (!is_slash(files[j].path, len_i + 1)) {
						filler(buf, files[j].path + len_i + 1, NULL, 0);
					}
				}
			}
			break;
		}
	}
	return is_counter(i);
}

static int q_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi) //чтение самого файла, открытие
{
	size_t len;
	(void)fi;

	int i;
	for (i = 0; i < _counter; i++) {
		if (!files[i].isDirectory && !strcmp(path, files[i].path)) {
			if (files[i].rights < 0400) {
				return -EACCES;
			}
			len = strlen(files[i].contents);
			if (offset < len) {
				if (offset + size > len)
					size = len - offset;
				memcpy(buf, files[i].contents + offset, size); //перенос читабельных данных на buf
			}
			else size = 0;
			return size;
		}
	}
	return -ENOENT;
}
void add_file_base(const char *path, int rights, bool isDirectory, char *contents) { //вспомогательная, для создания файлаа
	files[_counter].path = strdup(path); //strdup создает новую строку в куче 
	files[_counter].rights = rights;
	files[_counter].isDirectory = isDirectory;
	if (contents != NULL) {
		files[_counter].contents = strdup(contents);
	}
	_counter++;
}

void add_directory(const char *path, int rights) {
	add_file_base(path, rights, true, NULL);
}

void add_file(const char *path, int rights, char *contents) {
	add_file_base(path, rights, false, contents);
}

static int q_mkdir(const char *path, mode_t mode) { //создание папки
	bool isNameOk = false;
	int len_path = strlen(path);
	for (int i = 0; i < _counter; i++) {
		if (!strcmp(files[i].path, path)) {
			return -EEXIST;
		}
		int len = strlen(files[i].path);
		if (len_path > len && !strncmp(path, files[i].path, len)) {
			isNameOk = true;
		}
	}

	if (!isNameOk) {
		return -ENAMETOOLONG;
	}

	add_directory(path, mode);

	return 0;
}
static int q_rename(const char* from, const char* to) { //изменение имени 
    for(int i = 0; i < _counter; i++) {
        if(!strcmp(files[i].path, from)) {
		    files[i].path = strdup(to);
        }
    }
	return 0;
}

static struct fuse_operations operations = { //основная структура операций fuse
	.read = q_read,
	.readdir = q_readdir,
	.getattr = q_getattr,
	.mkdir = q_mkdir,
    .rename = q_rename,
};


int main(int argc, char *argv[])//подгрузка всего необходимого и создание шаблона системы
{
	static const char *system_echo_path = "/home/plastb1r/OS/lab5/mnt_fuse_not_save";
	// whitespace + \n = chinese
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
        // добавление директорий
	add_directory("/", 0777);
	add_directory("/bar", 0755);
	add_directory("/bar/baz", 0744);

	int bufferSize = 40000; // размер буфера 
	char echoBuffer[bufferSize];
	FILE *fecho = fopen(system_echo_path, "rb");
	unsigned char c;
	int i = 0;
	while (fread(&c, 1, 1, fecho)) {
		echoBuffer[i] = c;
		i++;
	}
	fclose(fecho);
    
    char str[51];
    for (int i = 0; i < 24; i++) {
        str[i * 2] = i;
        str[i * 2 + 1] = '\n';
    }
    str[50] = '\0';
        // добавление файлов
	add_file("/bar/baz/bin", 0177, echoBuffer);
	add_file("/bar/baz/readme.txt", 0544, "Student Kovaleva Anastasia 16160007\n");
	add_file("/bar/baz/example", 0555, "Hello World!\n");
	add_directory("/bar/baz/foo", 0711);
	add_file("/bar/baz/foo/cp", 0444, str);
	add_file("/bar/baz/foo/test.txt", 0777, "a\nb\nc\nd\ne\nf\nj\n");

	return fuse_main(args.argc, args.argv, &operations, NULL);
}


