#include "nm_internal.h"

static RuntimeVal *file_open(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  (void)env;
  if (arg_count != 2 || args[0]->type != STRING_T ||
      args[1]->type != STRING_T) {
    error("open() expects two string arguments: path and mode");
  }

  char *path = ((StringVal *)args[0])->value;
  char *mode = ((StringVal *)args[1])->value;

  FILE *fp = fopen(path, mode);
  if (!fp) {
    error("Does not possible open the file");
  }

  FileHandle *handle = malloc_safe(sizeof(FileHandle), "FileHandle");
  handle->fp = fp;
  handle->mode = strdup(mode);

  return (RuntimeVal *)handle;
}

static RuntimeVal *file_close(Environment *env, RuntimeVal **args,
                              size_t arg_count) {
  (void)env; (void)arg_count;
  FileHandle *handle = ((FileHandle *)args[0]);

  if (handle->fp == NULL) {
    error("The file is already closed");
  }

  if (strchr(handle->mode, 'w') != NULL || strchr(handle->mode, 'a') != NULL ||
      strchr(handle->mode, '+') != NULL) {
    fflush(handle->fp);
  }
  if (fclose(handle->fp) != 0) {
    error("Error closing the file");
  }
  free_safe(handle->mode);
  free_safe(handle);

  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *file_read(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  (void)env; (void)arg_count;
  FileHandle *handle = (FileHandle *)args[0];
  if (strcmp(handle->mode, "r") != 0 && strcmp(handle->mode, "r+") != 0) {
    error("The file does not open for reading");
  }

  fseek(handle->fp, 0, SEEK_END);
  long fsize = ftell(handle->fp);
  fseek(handle->fp, 0, SEEK_SET);

  if (fsize < 0) return (RuntimeVal *)MK_STRING("");

  char *content = malloc_safe(fsize + 1, "file_read content");
  size_t bytes_read = fread(content, 1, fsize, handle->fp);
  content[bytes_read] = '\0';

  RuntimeVal *res = (RuntimeVal *)MK_STRING(content);
  free_safe(content);
  return res;
}

static RuntimeVal *file_readline(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  (void)env; (void)arg_count;
  FileHandle *handle = (FileHandle *)args[0];
  if (strcmp(handle->mode, "r") != 0 && strcmp(handle->mode, "r+") != 0) {
    error("The file does not open for reading");
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  read = getline(&line, &len, handle->fp);
  if (read == -1) {
    free_safe(line);
    return (RuntimeVal *)MK_NUMBER(-1);
  }

  if (read > 0 && line[read - 1] == '\n') {
    line[read - 1] = '\0';
    read--;
  }

  RuntimeVal *res = (RuntimeVal *)MK_STRING(line);
  free_safe(line);
  return res;
}

static RuntimeVal *file_write(Environment *env, RuntimeVal **args,
                              size_t arg_count) {
  (void)env; (void)arg_count;
  FileHandle *handle = (FileHandle *)args[0];
  if (strcmp(handle->mode, "w") != 0 && strcmp(handle->mode, "w+") != 0 &&
      strcmp(handle->mode, "a") != 0 && strcmp(handle->mode, "a+") != 0) {
    error("The file does not open for writing");
  }

  char *content = ((StringVal *)args[1])->value;
  fputs(content, handle->fp);

  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *file_seek(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  (void)env; (void)arg_count;
  FileHandle *handle = (FileHandle *)args[0];
  long offset = (long)((NumberVal *)args[1])->value;

  fseek(handle->fp, offset, SEEK_SET);

  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *file_exists(Environment *env, RuntimeVal **args,
                               size_t arg_count) {
  (void)env; (void)arg_count;
  char *path = ((StringVal *)args[0])->value;
  FILE *file = fopen(path, "r");
  if (file != NULL) {
    fclose(file);
    return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

static RuntimeVal *file_delete(Environment *env, RuntimeVal **args,
                               size_t arg_count) {
  (void)env; (void)arg_count;
  char *path = ((StringVal *)args[0])->value;
  if (remove(path) == 0) {
    return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

static RuntimeVal *file_move(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  (void)env; (void)arg_count;
  char *path1 = ((StringVal *)args[0])->value;
  char *path2 = ((StringVal *)args[1])->value;

  if (rename(path1, path2) == 0) {
    return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

static RuntimeVal *file_copy(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  (void)env; (void)arg_count;
  char *path1 = ((StringVal *)args[0])->value;
  char *path2 = ((StringVal *)args[1])->value;

  FILE *source, *destination;
  char buffer[64 * 1024];
  size_t bytesRead;

  source = fopen(path1, "rb");
  if (source == NULL) {
    error("Does not possible open source the file");
    return (RuntimeVal *)MK_BOOL(0);
  }

  destination = fopen(path2, "wb");
  if (destination == NULL) {
    error("Does not possible open destination the file");
    fclose(source);
    return (RuntimeVal *)MK_BOOL(0);
  }

  while ((bytesRead = fread(buffer, 1, sizeof(buffer), source)) > 0) {
    fwrite(buffer, 1, bytesRead, destination);
  }

  fclose(source);
  fclose(destination);

  return (RuntimeVal *)MK_BOOL(1);
}

void init_file_module(Environment *env) {
  char *double_param[] = {"f", "n"};
  char *single_param[] = {"f"};

  declare_owned(env, "open",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_open));
  declare_owned(env, "fRead",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_read));
  declare_owned(env, "fExists",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_exists));
  declare_owned(env, "fDelete",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_delete));
  declare_owned(env, "fReadLine",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_readline));
  declare_owned(env, "fWrite",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_write));
  declare_owned(env, "fSeek",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_seek));
  declare_owned(env, "fCopy",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_copy));
  declare_owned(env, "fMove",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_move));
  declare_owned(env, "fClose",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_close));
}
