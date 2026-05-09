#include "../include/arena.h"
#include "../include/ast.h"
#include "../include/evaluator.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <limits.h>
#endif

int g_argc;
char **g_argv;

char nira_std_lib_path[1024] = "libs";
char nira_global_libs_path[1024] = ".nira/libs";

void resolve_std_lib_path() {
    char exe_dir[1024] = "";
#ifdef __APPLE__
    uint32_t size = sizeof(exe_dir);
    if (_NSGetExecutablePath(exe_dir, &size) != 0) {
        strcpy(exe_dir, "");
    }
#elif __linux__
    ssize_t count = readlink("/proc/self/exe", exe_dir, sizeof(exe_dir) - 1);
    if (count != -1) {
        exe_dir[count] = '\0';
    }
#endif
    if (exe_dir[0]) {
        char *last_slash = strrchr(exe_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            // libs/ is sibling to executable
            snprintf(nira_std_lib_path, sizeof(nira_std_lib_path), "%s/libs", exe_dir);
            snprintf(nira_global_libs_path, sizeof(nira_global_libs_path), "%s/.nira/libs", exe_dir);
        }
    }
}
void codegen_c_program(AstNode *node, FILE *out);
char* codegen_get_links();
void nr_add_include_path(const char *path);
void nr_eval_add_include_path(const char *path);

char *read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

void handle_install(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: nira install [-g] {user}/{repo}:{version}\n");
    return;
  }

  int is_global = 0;
  char *pkg = NULL;

  if (strcmp(argv[2], "-g") == 0) {
    is_global = 1;
    if (argc < 4) {
      printf("Usage: nira install -g {user}/{repo}:{version}\n");
      return;
    }
    pkg = argv[3];
  } else {
    pkg = argv[2];
  }

  printf("📦 Installing %s %s...\n", pkg, is_global ? "(global)" : "(local)");

  // Simple parsing of user/repo:version
  char* slash = strchr(pkg, '/');
  char* colon = strchr(pkg, ':');
  if (!slash || !colon) {
    printf("Error: Invalid package format. Use {user}/{repo}:{version}\n");
    return;
  }

  char repo_name[128] = {0};
  strncpy(repo_name, slash + 1, colon - slash - 1);

  char base_path[1024];
  if (is_global) {
    // Global: install to <nira_executable_dir>/.nira/libs/
    char nira_dir[1024];
    strncpy(nira_dir, nira_global_libs_path, sizeof(nira_dir));
    // nira_global_libs_path already points to <exe_dir>/.nira/libs
    char nira_dotdir[1024];
    // Create <exe_dir>/.nira/
    strncpy(nira_dotdir, nira_global_libs_path, sizeof(nira_dotdir));
    char *libs_suffix = strrchr(nira_dotdir, '/');
    if (libs_suffix) *libs_suffix = '\0';
    mkdir(nira_dotdir, 0755);
    mkdir(nira_global_libs_path, 0755);
    snprintf(base_path, sizeof(base_path), "%s/%s", nira_global_libs_path, repo_name);
  } else {
    // Local: install to <workspace>/.nira/libs/
    mkdir(".nira", 0755);
    mkdir(".nira/libs", 0755);
    snprintf(base_path, sizeof(base_path), ".nira/libs/%s", repo_name);
  }
  mkdir(base_path, 0755);

  if (!is_global) {
    // Create nira.json if it doesn't exist (local only)
    FILE *f = fopen("nira.json", "r");
    if (!f) {
      f = fopen("nira.json", "w");
      fprintf(f, "{\n  \"dependencies\": {\n    \"%s\": \"latest\"\n  }\n}\n", pkg);
      fclose(f);
      printf("📄 Created nira.json\n");
    } else {
      fclose(f);
    }
  }

  printf("✅ Installed %s to %s\n", pkg, base_path);
}

void handle_test(int argc, char **argv) {
  (void)argc; (void)argv;
  DIR *d = opendir("tests");
  if (!d) {
    printf("Error: Could not open tests/ directory\n");
    return;
  }

  struct dirent *dir;
  int passed = 0;
  int total = 0;
  char *test_files[256];

  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, ".nr") && !strstr(dir->d_name, "stress_all.nr")) {
      test_files[total++] = strdup(dir->d_name);
    }
  }
  closedir(d);

  // Sort files for consistent output
  for (int i = 0; i < total - 1; i++) {
    for (int j = i + 1; j < total; j++) {
      if (strcmp(test_files[i], test_files[j]) > 0) {
        char *temp = test_files[i];
        test_files[i] = test_files[j];
        test_files[j] = temp;
      }
    }
  }

  int is_build = (argc > 2 && strcmp(argv[2], "-b") == 0);
  
  printf("\n🚀 Running Nira Test Suite %s...\n", is_build ? "(Build Mode)" : "(Interpreter Mode)");
  printf("========================================\n");

  for (int i = 0; i < total; i++) {
    char cmd[2048];
    if (is_build) {
      char bin_name[256];
      strncpy(bin_name, test_files[i], sizeof(bin_name));
      char *dot = strrchr(bin_name, '.');
      if (dot) *dot = '\0';
      
      // Build first
      snprintf(cmd, sizeof(cmd), "./nira build tests/%s > /dev/null 2>&1", test_files[i]);
      int build_status = system(cmd);
      
      if (build_status == 0) {
        // Run the binary
        snprintf(cmd, sizeof(cmd), "./build/%s > /dev/null 2>&1", bin_name);
      } else {
        // Build failed, set a non-zero status to trigger FAILED
        build_status = 1;
      }
      
      printf("[%02d/%02d] Testing %-30s ", i + 1, total, test_files[i]);
      fflush(stdout);

      int status = (build_status == 0) ? system(cmd) : build_status;
      if (status == 0) {
        printf("\033[1;32mPASSED\033[0m\n");
        passed++;
      } else {
        printf("\033[1;31mFAILED\033[0m\n");
      }
    } else {
      snprintf(cmd, sizeof(cmd), "./nira run tests/%s > /dev/null 2>&1", test_files[i]);
      
      printf("[%02d/%02d] Testing %-30s ", i + 1, total, test_files[i]);
      fflush(stdout);

      int status = system(cmd);
      if (status == 0) {
        printf("\033[1;32mPASSED\033[0m\n");
        passed++;
      } else {
        printf("\033[1;31mFAILED\033[0m\n");
      }
    }
    free(test_files[i]);
  }

  printf("========================================\n");
  if (passed == total) {
    printf("✨ \033[1;32mALL TESTS PASSED\033[0m (%d/%d)\n\n", passed, total);
  } else {
    printf("❌ \033[1;31mSOME TESTS FAILED\033[0m (%d/%d passed)\n\n", passed, total);
  }
}

void print_help() {
  printf("Nira Programming Language CLI\n\n");
  printf("Usage:\n");
  printf("  nira run <file>       Transpile and run the script immediately\n");
  printf("  nira build <file>     Transpile and compile to a standalone binary\n");
  printf("  nira test             Run all tests in tests/ folder (Interpreter)\n");
  printf("  nira test -b          Run all tests in tests/ folder (AOT Build)\n");
  printf("  nira install <pkg>    Install a dependency locally\n");
  printf("  nira install -g <pkg> Install a dependency globally\n");
  printf("  nira help             Show this help message\n\n");
}

static pid_t g_child_pid = 0;

void handle_signal(int sig) {
  if (g_child_pid > 0) {
    kill(g_child_pid, SIGKILL);
  }
  exit(1);
}

int main(int argc, char *argv[]) {
  resolve_std_lib_path();
  g_argc = argc;
  g_argv = argv;
  nr_init_memory();
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  if (argc < 2) {
    print_help();
    return 0;
  }

  char *cmd = argv[1];
  char *filename = NULL;
  int is_run = 0;
  int is_build = 0;

  if (strcmp(cmd, "help") == 0) {
    print_help();
    return 0;
  } else if (strcmp(cmd, "install") == 0) {
    handle_install(argc, argv);
    return 0;
  } else if (strcmp(cmd, "test") == 0) {
    handle_test(argc, argv);
    return 0;
  } else if (strcmp(cmd, "run") == 0) {
    if (argc < 3) {
      printf("Error: No file specified for 'run'\n");
      return 1;
    }
    filename = argv[2];
    is_run = 1;
  } else if (strcmp(cmd, "build") == 0) {
    if (argc < 3) {
      printf("Error: No file specified for 'build'\n");
      return 1;
    }
    filename = argv[2];
    is_build = 1;
  } else {
    // Default to run if no command specified (backward compatibility)
    filename = argv[1];
    is_run = 1;
  }

  // Set include path based on filename
  char *dir = strdup(filename);
  char *last_slash = strrchr(dir, '/');
  if (last_slash) {
    *last_slash = '\0';
    nr_add_include_path(dir);
    nr_eval_add_include_path(dir);
  } else {
    nr_add_include_path(".");
    nr_eval_add_include_path(".");
  }
  free(dir);

  char *source = read_file(filename);
  if (!source) return 1;

  // Check if file is binary (ELF or Mach-O)
  unsigned char *bytes = (unsigned char *)source;
  if ((bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F') ||
      (bytes[0] == 0xCF && bytes[1] == 0xFA && bytes[2] == 0xED && bytes[3] == 0xFE) ||
      (bytes[0] == 0xFE && bytes[1] == 0xED && bytes[2] == 0xFA && bytes[3] == 0xCF) ||
      (bytes[0] == 0xCE && bytes[1] == 0xFA && bytes[2] == 0xED && bytes[3] == 0xFE)) {
    fprintf(stderr, "\033[1;31m[ERROR]\033[0m '%s' is a compiled binary.\n", filename);
    fprintf(stderr, "\033[1;34m-->\033[0m Run it directly: ./%s\n", filename);
    free(source);
    return 1;
  }

  Lexer lexer;
  lexer_init(&lexer, source);
  Parser parser;
  parser_init(&parser, &lexer, filename);
  AstNode *program = parse_program(&parser);
  if (parser.had_error) {
    return 1;
  }

  if (is_run) {
    nr_resolve(program);
    Environment *child_env = env_new(NULL, 0);
    child_env->source = source;
    child_env->filename = filename;
    nr_eval_add_include_path(".");
    nr_eval_add_include_path(nira_std_lib_path);
    eval(program, child_env);
    Value main_func = env_get(child_env, "main");
    if (main_func.type == VAL_FUNC) {
      AstNode *call_node = ast_new(AST_CALL, (Token){0});
      call_node->data.call.name = "main";
      call_node->data.call.args = NULL;
      call_node->data.call.arg_count = 0;
      eval(call_node, child_env);
    }
  } else if (is_build) {
    // Sanitize filename to prevent command injection
    for (char *p = filename; *p; p++) {
      if (!isalnum(*p) && *p != '.' && *p != '/' && *p != '_' && *p != '-') {
        fprintf(stderr,
                "Error: Illegal character '%c' in filename. Only alphanumeric, "
                "'.', '/', '_', and '-' are allowed.\n",
                *p);
        return 1;
      }
    }

    system("mkdir -p build");
    char out_name[256];
    snprintf(out_name, sizeof(out_name), "build/%s.c", filename);

    // Safely create directory for the .c file
    char dir_path[512];
    strncpy(dir_path, out_name, sizeof(dir_path));
    char *last_slash_build = strrchr(dir_path, '/');
    if (last_slash_build) {
      *last_slash_build = '\0';
      char mkdir_cmd[1024];
      snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", dir_path);
      system(mkdir_cmd);
    }

    FILE *out = fopen(out_name, "w");
    codegen_c_program(program, out);
    fclose(out);

    char bin_name[256];
    const char *base = strrchr(filename, '/');
    if (!base)
      base = filename;
    else
      base++;

    strncpy(bin_name, base, sizeof(bin_name));
    char *dot = strrchr(bin_name, '.');
    if (dot)
      *dot = '\0';

    char bin_path[512];
    snprintf(bin_path, sizeof(bin_path), "build/%s", bin_name);

    char compile_cmd[2048];
#ifdef __APPLE__
    snprintf(compile_cmd, sizeof(compile_cmd),
             "clang -w -Ofast -funroll-loops -march=native -ffast-math -o %s %s %s -I/opt/homebrew/include -L/opt/homebrew/lib", bin_path, out_name, codegen_get_links());
#else
    snprintf(compile_cmd, sizeof(compile_cmd),
             "clang -w -Ofast -funroll-loops -march=native -ffast-math -o %s %s %s", bin_path, out_name, codegen_get_links());
#endif

    int status = system(compile_cmd);
    if (status == 0) {
      struct stat st;
      if (stat(bin_path, &st) == 0) {
        printf("\n✨ Build Successful!\n");
        printf("   📦 Binary: %s\n", bin_path);
        printf("   📊 Size  : %.2f KB\n\n", (double)st.st_size / 1024.0);
      } else {
        printf("\n✨ Build Successful!\n");
        printf("   📦 Binary: %s\n\n", bin_path);
      }
    } else {
      printf("\n❌ Build Failed!\n");
      printf("   Check your Nira syntax and dependencies.\n\n");
    }
  }

  free(source);
  return 0;
}

void dump_ast(AstNode *node, int indent) {
  if (!node)
    return;
  for (int i = 0; i < indent; i++)
    printf("  ");

  switch (node->type) {
  case AST_PROGRAM:
    for (int i = 0; i < node->data.program.count; i++) {
      dump_ast(node->data.program.statements[i], indent + 1);
    }
    break;
  case AST_FUNC_DECL:
    printf("FuncDecl: %s (", node->data.func_decl.name);
    for (int i = 0; i < node->data.func_decl.param_count; i++) {
      printf("%s%s", node->data.func_decl.params[i],
             (i == node->data.func_decl.param_count - 1) ? "" : ", ");
    }
    printf(")\n");
    dump_ast(node->data.func_decl.body, indent + 1);
    break;
  case AST_ASSIGN:
    printf("Assign: %s =\n", node->data.assign.target);
    dump_ast(node->data.assign.value, indent + 1);
    break;
  case AST_VAR_REF:
    printf("VarRef: %s\n", node->data.var_ref.name);
    break;
  case AST_LITERAL_INT:
    printf("LiteralInt: %lld\n", node->data.int_val);
    break;
  case AST_LITERAL_STR:
    printf("LiteralStr: %s\n", node->data.str_val);
    break;
  case AST_OBJECT:
    printf("Object:\n");
    AstField *f = node->data.object.fields;
    while (f) {
      for (int i = 0; i < indent + 1; i++)
        printf("  ");
      printf("%s:\n", f->name);
      dump_ast(f->value, indent + 2);
      f = f->next;
    }
    break;
  case AST_CALL:
    printf("Call: %s (%d args)\n", node->data.call.name,
           node->data.call.arg_count);
    for (int i = 0; i < node->data.call.arg_count; i++) {
      dump_ast(node->data.call.args[i], indent + 1);
    }
    break;
  case AST_BINARY:
    printf("Binary: %u\n", (unsigned int)node->data.binary.op);
    dump_ast(node->data.binary.left, indent + 1);
    dump_ast(node->data.binary.right, indent + 1);
    break;
  case AST_IF:
    printf("If:\n");
    dump_ast(node->data.if_stmt.condition, indent + 1);
    dump_ast(node->data.if_stmt.then_branch, indent + 1);
    if (node->data.if_stmt.else_branch)
      dump_ast(node->data.if_stmt.else_branch, indent + 1);
    break;
  case AST_FOR:
    printf("For: %s\n", node->data.for_stmt.var);
    dump_ast(node->data.for_stmt.iterable, indent + 1);
    dump_ast(node->data.for_stmt.body, indent + 1);
    break;
  case AST_ARRAY:
    printf("Array: (%d elements)\n", node->data.array.count);
    for (int i = 0; i < node->data.array.count; i++) {
      dump_ast(node->data.array.elements[i], indent + 1);
    }
    break;
  case AST_INDEX:
    printf("Index:\n");
    dump_ast(node->data.index.object, indent + 1);
    dump_ast(node->data.index.index, indent + 1);
    break;
  case AST_INDEX_ASSIGN:
    printf("IndexAssign:\n");
    dump_ast(node->data.index_assign.object, indent + 1);
    dump_ast(node->data.index_assign.index, indent + 1);
    dump_ast(node->data.index_assign.value, indent + 1);
    break;
  case AST_RETURN:
    printf("Return:\n");
    dump_ast(node->data.ret.value, indent + 1);
    break;
  default:
    printf("Unknown node type: %d\n", node->type);
  }
}
