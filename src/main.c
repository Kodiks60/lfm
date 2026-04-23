#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>

// Цвета
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

// Пути
#define LFM_DB_DIR     "/var/lib/lfm/db"
#define LFM_BUILD_DIR  "/tmp/lfm-build"
#define LFM_FORMULA_URL "https://raw.githubusercontent.com/Kodiks60/lfm/main/formules"

// Функции
void print_help(void);
int cmd_install(const char *pkg_name);
int cmd_remove(const char *pkg_name);
int cmd_list(void);
int cmd_update(const char *pkg_name);
int download_formula(const char *pkg_name, char *formula_path);
int parse_formula(const char *formula_path, char *repo_url, char *build_cmd, char *install_cmd);
int run_command(const char *cmd);
int is_installed(const char *pkg_name);
void save_to_db(const char *pkg_name, const char *repo_url);

void print_help(void) {
    printf("%slfm - Leaffy Fast Manager%s\n", BOLD CYAN, RESET);
    printf("Package manager for Leaffy Linux\n\n");
    printf("%sCommands:%s\n", BOLD, RESET);
    printf("  lfm install  <pkg>   Install package (git clone + make + make install)\n");
    printf("  lfm remove   <pkg>   Remove package (make uninstall)\n");
    printf("  lfm list             List installed packages\n");
    printf("  lfm update   <pkg>   Update package (git pull + make + make install)\n");
    printf("  lfm help             Show this help\n\n");
    printf("%sExamples:%s\n", BOLD, RESET);
    printf("  lfm install lfetch\n");
    printf("  lfm remove lfetch\n");
    printf("  lfm list\n");
    printf("  lfm update lfetch\n");
}

int run_command(const char *cmd) {
    printf("%s> %s%s\n", CYAN, cmd, RESET);
    int result = system(cmd);
    if (result != 0) {
        printf("%sError: Command failed%s\n", RED, RESET);
        return -1;
    }
    return 0;
}

int download_formula(const char *pkg_name, char *formula_path) {
    char url[512];
    snprintf(url, sizeof(url), "%s/%s.lfm", LFM_FORMULA_URL, pkg_name);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s -o %s %s", formula_path, url);
    
    printf("%sDownloading formula for %s...%s\n", YELLOW, pkg_name, RESET);
    
    if (run_command(cmd) != 0) {
        return -1;
    }
    
    // Проверяем, что файл скачался
    FILE *fp = fopen(formula_path, "r");
    if (!fp) {
        printf("%sFormula not found for: %s%s\n", RED, pkg_name, RESET);
        return -1;
    }
    fclose(fp);
    
    return 0;
}

int parse_formula(const char *formula_path, char *repo_url, char *build_cmd, char *install_cmd) {
    FILE *fp = fopen(formula_path, "r");
    if (!fp) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Пропускаем пустые строки
        if (line[0] == '\n' || line[0] == '\r') continue;
        
        char key[64], value[512];
        // Ищем знак равенства
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        // Копируем ключ
        int key_len = eq - line;
        if (key_len >= (int)sizeof(key)) key_len = sizeof(key) - 1;
        strncpy(key, line, key_len);
        key[key_len] = '\0';
        
        // Копируем значение (убираем кавычки и пробелы)
        char *val_start = eq + 1;
        while (*val_start == ' ' || *val_start == '\t') val_start++;
        
        // Убираем кавычки в начале и конце
        if (*val_start == '"') val_start++;
        char *val_end = val_start + strlen(val_start) - 1;
        while (val_end > val_start && (*val_end == '"' || *val_end == '\n' || *val_end == '\r')) {
            *val_end = '\0';
            val_end--;
        }
        
        // Сравниваем ключи
        if (strcmp(key, "source") == 0) {
            strcpy(repo_url, val_start);
        } else if (strcmp(key, "build_cmd") == 0) {
            strcpy(build_cmd, val_start);
        } else if (strcmp(key, "install_cmd") == 0) {
            strcpy(install_cmd, val_start);
        }
    }
    fclose(fp);
    
    // Значения по умолчанию
    if (strlen(build_cmd) == 0) strcpy(build_cmd, "make");
    if (strlen(install_cmd) == 0) strcpy(install_cmd, "sudo make install");
    
    // Проверка, что repo_url не пустой
    if (strlen(repo_url) == 0) {
        printf("%sError: source not found in formula%s\n", RED, RESET);
        return -1;
    }
    
    return 0;
}

int is_installed(const char *pkg_name) {
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/%s", LFM_DB_DIR, pkg_name);
    struct stat st;
    return (stat(db_path, &st) == 0);
}

void save_to_db(const char *pkg_name, const char *repo_url) {
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/%s", LFM_DB_DIR, pkg_name);
    
    // Создаём директорию базы данных (если нет)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sudo mkdir -p %s", LFM_DB_DIR);
    system(cmd);
    
    // СОЗДАЁМ ПАПКУ ПАКЕТА (это важно!)
    snprintf(cmd, sizeof(cmd), "sudo mkdir -p %s", db_path);
    system(cmd);
    
    // Сохраняем информацию о пакете
    snprintf(cmd, sizeof(cmd), "echo '%s' | sudo tee %s/repo", repo_url, db_path);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "date | sudo tee %s/installed", db_path);
    system(cmd);
    
    printf("%sPackage saved to database%s\n", GREEN, RESET);
}

void remove_from_db(const char *pkg_name) {
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/%s", LFM_DB_DIR, pkg_name);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sudo rm -rf %s", db_path);
    system(cmd);
    
    printf("%sPackage removed from database%s\n", GREEN, RESET);
}

int cmd_install(const char *pkg_name) {
    // Проверяем, не установлен ли уже
    if (is_installed(pkg_name)) {
        printf("%sPackage %s is already installed%s\n", YELLOW, pkg_name, RESET);
        printf("Use 'lfm update %s' to update\n", pkg_name);
        return 0;
    }
    
    char formula_path[256];
    snprintf(formula_path, sizeof(formula_path), "/tmp/%s.lfm", pkg_name);
    
    // Скачиваем формулу
    if (download_formula(pkg_name, formula_path) != 0) {
        return -1;
    }
    
    // Парсим формулу
    char repo_url[512] = "";
    char build_cmd[256] = "";
    char install_cmd[256] = "";
    if (parse_formula(formula_path, repo_url, build_cmd, install_cmd) != 0) {
        printf("%sFailed to parse formula%s\n", RED, RESET);
        return -1;
    }
    
    printf("%sInstalling %s...%s\n", GREEN, pkg_name, RESET);
    
    // Создаём временную директорию
    char build_dir[512];
    snprintf(build_dir, sizeof(build_dir), "%s/%s", LFM_BUILD_DIR, pkg_name);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sudo rm -rf %s && mkdir -p %s", build_dir, build_dir);
    run_command(cmd);
    
    // git clone
    snprintf(cmd, sizeof(cmd), "git clone %s %s", repo_url, build_dir);
    if (run_command(cmd) != 0) {
        printf("%sFailed to clone repository%s\n", RED, RESET);
        return -1;
    }
    
    // Переходим в директорию и собираем
    snprintf(cmd, sizeof(cmd), "cd %s && %s", build_dir, build_cmd);
    if (run_command(cmd) != 0) {
        printf("%sBuild failed%s\n", RED, RESET);
        return -1;
    }
    
    // Устанавливаем
    snprintf(cmd, sizeof(cmd), "cd %s && %s", build_dir, install_cmd);
    if (run_command(cmd) != 0) {
        printf("%sInstallation failed%s\n", RED, RESET);
        return -1;
    }
    
    // Сохраняем в базу
    save_to_db(pkg_name, repo_url);
    
    // Очищаем
    snprintf(cmd, sizeof(cmd), "rm -rf %s", build_dir);
    run_command(cmd);
    unlink(formula_path);
    
    printf("%s%s installed successfully!%s\n", GREEN BOLD, pkg_name, RESET);
    return 0;
}

int cmd_remove(const char *pkg_name) {
    if (!is_installed(pkg_name)) {
        printf("%sPackage %s is not installed%s\n", RED, pkg_name, RESET);
        return -1;
    }
    
    // Читаем repo_url из базы
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/%s/repo", LFM_DB_DIR, pkg_name);
    
    FILE *fp = fopen(db_path, "r");
    if (!fp) {
        printf("%sFailed to read package info%s\n", RED, RESET);
        return -1;
    }
    
    char repo_url[512];
    fgets(repo_url, sizeof(repo_url), fp);
    fclose(fp);
    
    // Убираем换行符
    repo_url[strcspn(repo_url, "\n")] = '\0';
    
    printf("%sRemoving %s...%s\n", YELLOW, pkg_name, RESET);
    
    // Клонируем во временную папку для uninstall
    char build_dir[512];
    snprintf(build_dir, sizeof(build_dir), "%s/%s", LFM_BUILD_DIR, pkg_name);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sudo rm -rf %s && mkdir -p %s", build_dir, build_dir);
    run_command(cmd);
    
    snprintf(cmd, sizeof(cmd), "git clone %s %s", repo_url, build_dir);
    if (run_command(cmd) != 0) {
        printf("%sFailed to clone for uninstall%s\n", RED, RESET);
        return -1;
    }
    
    // Выполняем make uninstall
    snprintf(cmd, sizeof(cmd), "cd %s && sudo make uninstall", build_dir);
    run_command(cmd);
    
    // Очищаем
    snprintf(cmd, sizeof(cmd), "rm -rf %s", build_dir);
    run_command(cmd);
    
    // Удаляем из базы
    remove_from_db(pkg_name);
    
    printf("%s%s removed successfully!%s\n", GREEN BOLD, pkg_name, RESET);
    return 0;
}

int cmd_list(void) {
    printf("%sInstalled packages:%s\n", BOLD CYAN, RESET);
    
    DIR *dir = opendir(LFM_DB_DIR);
    if (!dir) {
        printf("  No packages installed\n");
        return 0;
    }
    
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        printf("  %s✓%s %s\n", GREEN, RESET, entry->d_name);
        count++;
    }
    closedir(dir);
    
    if (count == 0) {
        printf("  No packages installed\n");
    } else {
        printf("\n  Total: %d\n", count);
    }
    
    return 0;
}

int cmd_update(const char *pkg_name) {
    if (!is_installed(pkg_name)) {
        printf("%sPackage %s is not installed%s\n", RED, pkg_name, RESET);
        return -1;
    }
    
    // Читаем repo_url из базы
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/%s/repo", LFM_DB_DIR, pkg_name);
    
    FILE *fp = fopen(db_path, "r");
    if (!fp) {
        printf("%sFailed to read package info%s\n", RED, RESET);
        return -1;
    }
    
    char repo_url[512];
    fgets(repo_url, sizeof(repo_url), fp);
    fclose(fp);
    repo_url[strcspn(repo_url, "\n")] = '\0';
    
    printf("%sUpdating %s...%s\n", GREEN, pkg_name, RESET);
    
    char build_dir[512];
    snprintf(build_dir, sizeof(build_dir), "%s/%s", LFM_BUILD_DIR, pkg_name);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "sudo rm -rf %s && mkdir -p %s", build_dir, build_dir);
    run_command(cmd);
    
    // git clone
    snprintf(cmd, sizeof(cmd), "git clone %s %s", repo_url, build_dir);
    if (run_command(cmd) != 0) {
        printf("%sFailed to clone%s\n", RED, RESET);
        return -1;
    }
    
    // make
    snprintf(cmd, sizeof(cmd), "cd %s && make", build_dir);
    if (run_command(cmd) != 0) {
        printf("%sBuild failed%s\n", RED, RESET);
        return -1;
    }
    
    // sudo make install
    snprintf(cmd, sizeof(cmd), "cd %s && sudo make install", build_dir);
    if (run_command(cmd) != 0) {
        printf("%sInstallation failed%s\n", RED, RESET);
        return -1;
    }
    
    // Обновляем дату установки
    snprintf(cmd, sizeof(cmd), "date | sudo tee %s/installed", db_path);
    system(cmd);
    
    // Очищаем
    snprintf(cmd, sizeof(cmd), "rm -rf %s", build_dir);
    run_command(cmd);
    
    printf("%s%s updated successfully!%s\n", GREEN BOLD, pkg_name, RESET);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }
    
    // Создаём нужные директории
    system("sudo mkdir -p /var/lib/lfm/db");
    system("mkdir -p /tmp/lfm-build");
    
    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            printf("%sUsage: lfm install <package>%s\n", RED, RESET);
            return 1;
        }
        return cmd_install(argv[2]);
        
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            printf("%sUsage: lfm remove <package>%s\n", RED, RESET);
            return 1;
        }
        return cmd_remove(argv[2]);
        
    } else if (strcmp(argv[1], "list") == 0) {
        return cmd_list();
        
    } else if (strcmp(argv[1], "update") == 0) {
        if (argc < 3) {
            printf("%sUsage: lfm update <package>%s\n", RED, RESET);
            return 1;
        }
        return cmd_update(argv[2]);
        
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
        
    } else {
        printf("%sUnknown command: %s%s\n", RED, argv[1], RESET);
        print_help();
        return 1;
    }
    
    return 0;
}
