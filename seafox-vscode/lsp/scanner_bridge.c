#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct
{
	int type;
	const char* start;
	int position;
	int length;
	int line;
} Token;

typedef void (*InitScannerFn)(const char*);
typedef Token (*ScanFn)(void);

static char* read_stdin(size_t* length_out) {
	size_t capacity = 4096;
	size_t length = 0;
	char* buffer = (char*) malloc(capacity);
	if (buffer == NULL) {
		return NULL;
	}

	while (1) {
		if (length + 1 >= capacity) {
			capacity *= 2;
			char* next = (char*) realloc(buffer, capacity);
			if (next == NULL) {
				free(buffer);
				return NULL;
			}
			buffer = next;
		}

		size_t read_bytes = fread(buffer + length, 1, capacity - length - 1, stdin);
		length += read_bytes;
		if (read_bytes == 0) {
			break;
		}
	}

	buffer[length] = '\0';
	*length_out = length;
	return buffer;
}

static char* build_library_path(const char* binary_path) {
	const char* slash = strrchr(binary_path, '/');
	size_t dir_len = slash ? (size_t) (slash - binary_path + 1) : 0;
	const char* library_name = "libscanner.so";
	size_t path_len = dir_len + strlen(library_name) + 1;
	char* path = (char*) malloc(path_len);
	if (path == NULL) {
		return NULL;
	}

	if (dir_len > 0) {
		memcpy(path, binary_path, dir_len);
	}
	strcpy(path + dir_len, library_name);
	return path;
}

static void write_json_string(FILE* stream, const char* text) {
	fputc('"', stream);
	for (const unsigned char* cursor = (const unsigned char*) text; *cursor != '\0'; ++cursor) {
		switch (*cursor) {
			case '\\':
				fputs("\\\\", stream);
				break;
			case '"':
				fputs("\\\"", stream);
				break;
			case '\n':
				fputs("\\n", stream);
				break;
			case '\r':
				fputs("\\r", stream);
				break;
			case '\t':
				fputs("\\t", stream);
				break;
			default:
				if (*cursor < 0x20) {
					fprintf(stream, "\\u%04x", *cursor);
				}
				else {
					fputc(*cursor, stream);
				}
				break;
		}
	}
	fputc('"', stream);
}

static void write_token_json(const Token* token, const char* source_text) {
	const char* token_text = source_text + token->position;
	size_t token_length = (size_t) token->length;
	if (token->length < 0) {
		token_length = 0;
	}

	fprintf(stdout, "{\"type\":%d,\"text\":", token->type);
	if (token_length > 0) {
		char* buffer = (char*) malloc(token_length + 1);
		if (buffer != NULL) {
			memcpy(buffer, token_text, token_length);
			buffer[token_length] = '\0';
			write_json_string(stdout, buffer);
			free(buffer);
		}
		else {
			fputc('"', stdout);
			fputc('"', stdout);
		}
	}
	else {
		fputc('"', stdout);
		fputc('"', stdout);
	}
	fprintf(stdout, ",\"line\":%d,\"position\":%d,\"length\":%d}\n", token->line, token->position, token->length);
}

int main(int argc, char** argv) {
	const char* binary_path = argc > 0 && argv[0] != NULL ? argv[0] : ".";
	char* library_path = build_library_path(binary_path);
	if (library_path == NULL) {
		fprintf(stderr, "failed to allocate library path\n");
		return 1;
	}

	void* handle = dlopen(library_path, RTLD_NOW | RTLD_LOCAL);
	if (handle == NULL) {
		fprintf(stderr, "failed to load %s: %s\n", library_path, dlerror());
		free(library_path);
		return 1;
	}

	InitScannerFn init_scanner = (InitScannerFn) dlsym(handle, "initScanner");
	ScanFn scan = (ScanFn) dlsym(handle, "scan");
	if (init_scanner == NULL || scan == NULL) {
		fprintf(stderr, "failed to resolve scanner entry points\n");
		dlclose(handle);
		free(library_path);
		return 1;
	}

	size_t source_length = 0;
	char* source_text = read_stdin(&source_length);
	if (source_text == NULL) {
		fprintf(stderr, "failed to read source from stdin\n");
		dlclose(handle);
		free(library_path);
		return 1;
	}

	init_scanner(source_text);

	while (1) {
		Token token = scan();
		write_token_json(&token, source_text);
		if (token.type == 60) {
			break;
		}
	}

	free(source_text);
	free(library_path);
	dlclose(handle);
	return 0;
}
