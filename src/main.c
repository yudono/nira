#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/ast.h"
#include "../include/evaluator.h"

void codegen_c_program(AstNode* node, FILE* out);
void nr_add_include_path(const char* path);
void nr_eval_add_include_path(const char* path);

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
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

void print_help() {
    printf("Nira Programming Language CLI\n\n");
    printf("Usage:\n");
    printf("  nira run <file>    Transpile and run the script immediately\n");
    printf("  nira build <file>  Transpile and compile to a standalone binary\n");
    printf("  nira help          Show this help message\n\n");
}

static pid_t g_child_pid = 0;

void handle_signal(int sig) {
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGKILL);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (argc < 2) {
        print_help();
        return 0;
    }

    char* cmd = argv[1];
    char* filename = NULL;
    int is_run = 0;
    int is_build = 0;

    if (strcmp(cmd, "help") == 0) {
        print_help();
        return 0;
    } else if (strcmp(cmd, "test") == 0) {
        printf("Running all tests...\n");
        // Simple hack: use system to find and run all .nr in tests/
        system("mkdir -p .temp && for f in tests/*.nr; do echo \"Testing $f...\"; ./nira run $f; done");
        return 0;
    } else if (strcmp(cmd, "run") == 0) {
        if (argc < 3) { printf("Error: No file specified for 'run'\n"); return 1; }
        filename = argv[2];
        is_run = 1;
    } else if (strcmp(cmd, "build") == 0) {
        if (argc < 3) { printf("Error: No file specified for 'build'\n"); return 1; }
        filename = argv[2];
        is_build = 1;
    } else {
        // Default to run if no command specified (backward compatibility)
        filename = argv[1];
        is_run = 1;
    }

    // Set include path based on filename
    char* dir = strdup(filename);
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        nr_add_include_path(dir);
        nr_eval_add_include_path(dir);
    } else {
        nr_add_include_path(".");
        nr_eval_add_include_path(".");
    }
    free(dir);

    char* source = read_file(filename);
    Lexer lexer;
    lexer_init(&lexer, source);
    Parser parser;
    parser_init(&parser, &lexer);
    AstNode* program = parse_program(&parser);
    if (parser.had_error) {
        return 1;
    }

    if (is_run) {
        Environment* env = env_new(NULL);
        env->source = source;
        env->filename = filename;
        while (1) {
            struct stat st;
            stat(filename, &st);
            long last_mod = st.st_mtime;

            g_child_pid = fork();
            if (g_child_pid == 0) {
                // Child: run the code
                Environment* child_env = env_new(NULL);
                child_env->source = source;
                child_env->filename = filename;
                nr_eval_add_include_path(".");
                nr_eval_add_include_path("lib");
                eval(program, child_env);
                Value main_func = env_get(child_env, "main");
                if (main_func.type == VAL_FUNC) {
                    AstNode* call_node = ast_new(AST_CALL, (Token){0});
                    call_node->data.call.name = "main";
                    call_node->data.call.args = NULL;
                    call_node->data.call.arg_count = 0;
                    eval(call_node, child_env);
                }
                exit(0);
            } else {
                // Parent: watch file
                printf("🚀 Running %s (Live Reload enabled)\n", filename);
                while (1) {
                    sleep(1);
                    struct stat st2;
                    stat(filename, &st2);
                    if (st2.st_mtime > last_mod) {
                        printf("\n🔄 File changed, restarting...\n");
                        kill(g_child_pid, SIGKILL);
                        int status;
                        waitpid(g_child_pid, &status, 0);
                        break; // Restart the outer loop
                    }
                    int status;
                    pid_t result = waitpid(g_child_pid, &status, WNOHANG);
                    if (result != 0) {
                        // Child finished or crashed
                        if (WIFEXITED(status)) {
                            if (WEXITSTATUS(status) != 0) printf("❌ Server exited with error code %d. Waiting for changes...\n", WEXITSTATUS(status));
                        } else if (WIFSIGNALED(status)) {
                            printf("❌ Server crashed (signal %d). Waiting for changes...\n", WTERMSIG(status));
                        }
                        
                        // Keep watching file
                        while (1) {
                            sleep(1);
                            struct stat st2;
                            stat(filename, &st2);
                            if (st2.st_mtime > last_mod) break; // Re-read and restart
                        }
                        break; 
                    }
                }
                // Re-read and re-parse before restarting
                source = read_file(filename);
                lexer_init(&lexer, source);
                parser_init(&parser, &lexer);
                program = parse_program(&parser);
            }
        }
    } else if (is_build) {
        system("mkdir -p build");
        char out_name[256];
        snprintf(out_name, sizeof(out_name), "build/%s.c", filename);
        // Create directory for the .c file if needed
        char mkdir_cmd[512];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p build/$(dirname %s)", filename);
        system(mkdir_cmd);
        
        FILE* out = fopen(out_name, "w");
        codegen_c_program(program, out);
        fclose(out);
        
        char bin_name[256];
        const char* base = strrchr(filename, '/');
        if (!base) base = filename;
        else base++;
        
        strncpy(bin_name, base, sizeof(bin_name));
        char* dot = strrchr(bin_name, '.');
        if (dot) *dot = '\0';
        
        char bin_path[512];
        snprintf(bin_path, sizeof(bin_path), "build/%s", bin_name);
        
        char compile_cmd[1024];
        snprintf(compile_cmd, sizeof(compile_cmd), "clang -w -O3 -o %s %s -lsqlite3", bin_path, out_name);
        
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

void dump_ast(AstNode* node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->type) {
        case AST_PROGRAM:
            printf("Program:\n");
            for (int i = 0; i < node->data.program.count; i++) {
                dump_ast(node->data.program.statements[i], indent + 1);
            }
            break;
        case AST_FUNC_DECL:
            printf("FuncDecl: %s (", node->data.func_decl.name);
            for (int i = 0; i < node->data.func_decl.param_count; i++) {
                printf("%s%s", node->data.func_decl.params[i], (i == node->data.func_decl.param_count - 1) ? "" : ", ");
            }
            printf(")\n");
            dump_ast(node->data.func_decl.body, indent + 1);
            break;
        case AST_ASSIGN:
            printf("Assign: %s =\n", node->data.assign.target);
            dump_ast(node->data.assign.value, indent + 1);
            break;
        case AST_VAR_REF:
            printf("VarRef: %s\n", node->data.var_name);
            break;
        case AST_LITERAL_INT:
            printf("LiteralInt: %d\n", node->data.int_val);
            break;
        case AST_LITERAL_STR:
            printf("LiteralStr: %s\n", node->data.str_val);
            break;
        case AST_OBJECT:
            printf("Object:\n");
            AstField* f = node->data.object.fields;
            while (f) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("%s:\n", f->name);
                dump_ast(f->value, indent + 2);
                f = f->next;
            }
            break;
        case AST_CALL:
            printf("Call: %s (%d args)\n", node->data.call.name, node->data.call.arg_count);
            for (int i=0; i<node->data.call.arg_count; i++) {
                dump_ast(node->data.call.args[i], indent + 1);
            }
            break;
        case AST_BINARY:
            printf("Binary: %s\n", node->data.binary.op);
            dump_ast(node->data.binary.left, indent + 1);
            dump_ast(node->data.binary.right, indent + 1);
            break;
        case AST_IF:
            printf("If:\n");
            dump_ast(node->data.if_stmt.condition, indent + 1);
            dump_ast(node->data.if_stmt.then_branch, indent + 1);
            if (node->data.if_stmt.else_branch) dump_ast(node->data.if_stmt.else_branch, indent + 1);
            break;
        case AST_FOR:
            printf("For: %s\n", node->data.for_stmt.var);
            dump_ast(node->data.for_stmt.iterable, indent + 1);
            dump_ast(node->data.for_stmt.body, indent + 1);
            break;
        case AST_ARRAY:
            printf("Array: (%d elements)\n", node->data.array.count);
            for (int i=0; i<node->data.array.count; i++) {
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
