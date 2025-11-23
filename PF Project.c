#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#include <time.h>
static void sleep_ms(int ms) {
    struct timespec ts; 
    ts.tv_sec = ms/1000; 
    ts.tv_nsec = (ms%1000)*1000000;
    nanosleep(&ts, NULL);
}
#endif

#define USERS_FILE "users.csv"
#define MAX_LINE 1024
#define MAX_CATS 100
#define MAX_CAT_LEN 64

// ANSI color codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define CYAN    "\033[36m"
#define YELLOW  "\033[33m"
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define MAGENTA "\033[35m"
#define WHITE   "\033[37m"

// ---------------------- Structs ----------------------

typedef struct {
    int id;
    char username[64];
    char password_enc[128]; // XOR-obfuscated
} User;

typedef struct {
    int id;
    char type[12];     // "Income" or "Expense"
    char category[64];
    double amount;
    char date[16];     // "DD/MM/YYYY"
    char note[256];
} Transaction;

typedef struct {
    Transaction *arr;
    int size;
    int cap;
} TxnList;

// ---------------------- Utilities ----------------------

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void safe_input(char *buf, int sz) {
    if (!fgets(buf, sz, stdin)) { 
        buf[0] = '\0'; 
        return; 
    }
    size_t l = strlen(buf); 
    if (l && buf[l-1] == '\n') 
        buf[l-1] = '\0';
}

void trim_newline(char *s) {
    if (!s) return; 
    size_t l = strlen(s); 
    if (l && s[l-1]=='\n') 
        s[l-1]=0;
}

void replace_commas(char *s) {
    for (; *s; ++s) 
        if (*s == ',') 
            *s = ';';
}

int parse_month(const char *date) {
    int d,m,y; 
    if (sscanf(date, "%d/%d/%d", &d, &m, &y) == 3) 
        return m; 
    return 0;
}

int parse_year(const char *date) {
    int d,m,y; 
    if (sscanf(date, "%d/%d/%d", &d, &m, &y) == 3) 
        return y; 
    return 0;
}

int date_valid(const char *date) {
    int d,m,y; 
    if (sscanf(date, "%d/%d/%d", &d, &m, &y) != 3) 
        return 0;
    if (m<1||m>12) return 0; 
    if (d<1||d>31) return 0; 
    if (y<1900||y>9999) return 0;
    return 1;
}

// Simple typing animation
void type_print(const char *s, int ms_delay) {
    const char *p;
    for (p = s; *p; ++p) { 
        putchar(*p); 
        fflush(stdout); 
        sleep_ms(ms_delay); 
    }
    putchar('\n');
}

// Small spinner
void spinner(int cycles) {
    const char seq[] = "|/-\\";
    int i;
    for (i=0;i<cycles;i++) {
        printf("\r%sLoading %c%s", CYAN, seq[i%4], RESET);
        fflush(stdout); 
        sleep_ms(100);
    }
    printf("\r                     \r");
}

// ---------------------- XOR password ----------------------

// XOR key -- single char; ok for obfuscation at PF-level
static const char XOR_KEY = 0x5A;

void xor_obfuscate(char *s) {
	int i;
    for (i=0; s[i]; ++i) 
        s[i] ^= XOR_KEY;
}

// ---------------------- TxnList management ----------------------

void init_txnlist(TxnList *t) { 
    t->arr = NULL; 
    t->size = 0; 
    t->cap = 0; 
}

void free_txnlist(TxnList *t) { 
    free(t->arr); 
    t->arr = NULL; 
    t->size = t->cap = 0; 
}

void ensure_txncap(TxnList *t, int need) {
    if (t->cap >= need) return;
    int nc = t->cap ? t->cap*2 : 8; 
    if (nc < need) nc = need;
    Transaction *tmp = realloc(t->arr, sizeof(Transaction)*nc);
    if (!tmp) { 
        printf("Memory error\n"); 
        exit(1); 
    }
    t->arr = tmp; 
    t->cap = nc;
}

void push_txn(TxnList *t, Transaction tx) { 
    ensure_txncap(t, t->size+1); 
    t->arr[t->size++] = tx; 
}

int next_txn_id(TxnList *t) {
    int i, mx = 0; 
    for (i=0;i<t->size;i++) 
        if (t->arr[i].id > mx) 
            mx = t->arr[i].id;
    return mx + 1;
}

// ---------------------- File paths ----------------------

void user_transactions_file(const char *username, char *out, size_t outsz) {
    snprintf(out, outsz, "user_%s.csv", username);
}

void user_settings_file(const char *username, char *out, size_t outsz) {
    snprintf(out, outsz, "user_%s_settings.txt", username);
}

// ---------------------- Users management (XOR encrypted pass) ----------------------

int user_exists(const char *username) {
    FILE *f = fopen(USERS_FILE, "r"); 
    if (!f) return 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (strlen(line)==0) continue;
        // CSV: id,username,encpass
        char *p = strchr(line, ','); 
        if (!p) continue;
        p++;
        char uname[64]; 
        int id; // read until next comma
        sscanf(p, "%63[^,],", uname);
        if (strcmp(uname, username) == 0) { 
            fclose(f); 
            return 1; 
        }
    }
    fclose(f); 
    return 0;
}

// generate next user id
int next_user_id() {
    FILE *f = fopen(USERS_FILE, "r"); 
    if (!f) return 1;
    char line[MAX_LINE]; 
    int last = 0;
    while (fgets(line, sizeof(line), f)) {
        int id; 
        if (sscanf(line, "%d,", &id) == 1) 
            last = id;
    }
    fclose(f); 
    return last+1;
}

int register_user(const char *username, const char *password) {
    if (user_exists(username)) return 0;
    User u; 
    u.id = next_user_id(); 
    strncpy(u.username, username, sizeof(u.username)-1);
    strncpy(u.password_enc, password, sizeof(u.password_enc)-1);
    xor_obfuscate(u.password_enc);
    FILE *f = fopen(USERS_FILE, "a");
    if (!f) return 0;
    // write CSV: id,username,encpass
    fprintf(f, "%d,%s,%s\n", u.id, u.username, u.password_enc);
    fclose(f);
    // create empty user files
    char path[256];
    user_transactions_file(username, path, sizeof(path));
    FILE *g = fopen(path, "w"); 
    if (g) fclose(g);
    user_settings_file(username, path, sizeof(path));
    g = fopen(path, "w"); 
    if (g) { 
        fprintf(g,"budget_limit:0.00\n"); 
        fclose(g); 
    }
    return 1;
}

int verify_user(const char *username, const char *password) {
    FILE *f = fopen(USERS_FILE, "r"); 
    if (!f) return 0;
    char line[MAX_LINE];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line); 
        if (strlen(line)==0) continue;
        int id; 
        char uname[64], enc[128];
        // parse id,username,enc
        char *p1 = strtok(line, ","); 
        char *p2 = strtok(NULL, ","); 
        char *p3 = strtok(NULL, ",");
        if (!p1 || !p2 || !p3) continue;
        id = atoi(p1); 
        strncpy(uname, p2, sizeof(uname)-1); 
        uname[sizeof(uname)-1]=0;
        strncpy(enc, p3, sizeof(enc)-1); 
        enc[sizeof(enc)-1]=0;
        if (strcmp(uname, username) == 0) {
            // de-obfuscate a copy
            char test[128]; 
            strncpy(test, enc, sizeof(test)-1);
            xor_obfuscate(test);
            if (strcmp(test, password) == 0) ok = 1;
            break;
        }
    }
    fclose(f); 
    return ok;
}

int change_user_password(const char *username, const char *oldpass, const char *newpass) {
    // verify first
    if (!verify_user(username, oldpass)) return 0;
    FILE *f = fopen(USERS_FILE, "r"); 
    if (!f) return 0;
    FILE *g = fopen("users_tmp.csv", "w"); 
    if (!g) { 
        fclose(f); 
        return 0; 
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line); 
        if (strlen(line)==0) continue;
        char copy[MAX_LINE]; 
        strncpy(copy, line, sizeof(copy)-1);
        char *p1 = strtok(copy, ","); 
        char *p2 = strtok(NULL, ","); 
        char *p3 = strtok(NULL, ",");
        if (!p1 || !p2) continue;
        if (strcmp(p2, username) == 0) {
            char encnew[128]; 
            strncpy(encnew, newpass, sizeof(encnew)-1); 
            xor_obfuscate(encnew);
            fprintf(g, "%s,%s,%s\n", p1, p2, encnew);
        } else {
            // write original line
            fprintf(g, "%s\n", line);
        }
    }
    fclose(f); 
    fclose(g);
    remove(USERS_FILE); 
    rename("users_tmp.csv", USERS_FILE);
    return 1;
}

// ---------------------- Transactions load/save ----------------------

void load_transactions(const char *username, TxnList *list) {
    init_txnlist(list);
    char path[256]; 
    user_transactions_file(username, path, sizeof(path));
    FILE *f = fopen(path, "r"); 
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line); 
        if (strlen(line) == 0) continue;
        // CSV: id,type,category,amount,date,note
        Transaction t; 
        char *tok = strtok(line, ","); 
        if (!tok) continue; 
        t.id = atoi(tok);
        tok = strtok(NULL, ","); 
        if (!tok) continue; 
        strncpy(t.type, tok, sizeof(t.type)-1);
        tok = strtok(NULL, ","); 
        if (!tok) continue; 
        strncpy(t.category, tok, sizeof(t.category)-1);
        tok = strtok(NULL, ","); 
        if (!tok) continue; 
        t.amount = atof(tok);
        tok = strtok(NULL, ","); 
        if (!tok) continue; 
        strncpy(t.date, tok, sizeof(t.date)-1);
        tok = strtok(NULL, ""); 
        if (tok) { 
            trim_newline(tok); 
            strncpy(t.note, tok, sizeof(t.note)-1); 
        } else 
            t.note[0] = '\0';
        push_txn(list, t);
    }
    fclose(f);
}

int save_transactions(const char *username, TxnList *list) {
    char path[256]; 
    user_transactions_file(username, path, sizeof(path));
    FILE *f = fopen(path, "w"); 
    if (!f) return 0;
    int i;
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i]; 
        char note_safe[256]; 
        strncpy(note_safe, t->note, sizeof(note_safe)-1);
        replace_commas(note_safe);
        fprintf(f, "%d,%s,%s,%.2f,%s,%s\n", t->id, t->type, t->category, t->amount, t->date, note_safe);
    }
    fclose(f); 
    return 1;
}

// Settings
double load_budget_limit(const char *username) {
    char path[256]; 
    user_settings_file(username, path, sizeof(path));
    FILE *f = fopen(path, "r"); 
    if (!f) return 0.0;
    char line[256]; 
    double lim = 0.0;
    if (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, ':'); 
        if (p) lim = atof(p+1);
    }
    fclose(f); 
    return lim;
}

void save_budget_limit(const char *username, double v) {
    char path[256]; 
    user_settings_file(username, path, sizeof(path));
    FILE *f = fopen(path, "w"); 
    if (!f) return;
    fprintf(f, "budget_limit:%.2f\n", v);
    fclose(f);
}

// ---------------------- Categories (defaults) ----------------------

int load_default_categories(char cats[][MAX_CAT_LEN]) {
    const char *defs[] = {
        "Grocery", "Utilities", "Transportation", 
        "Dining & Food", "Shopping", "Others"
    };
    int n = sizeof(defs)/sizeof(defs[0]);
    int i;
    for (i=0;i<n;i++) 
        strncpy(cats[i], defs[i], MAX_CAT_LEN-1);
    return n;
}

// ---------------------- UI Boxes ----------------------

void print_box(const char *title, const char *lines[], int nlines, const char *color) {
    printf("%s", color);
    printf("+"); 
    int i,j;
    for (i=0;i<60;i++) putchar('-'); 
    printf("+\n");
    int pad = (60 - (int)strlen(title)) / 2;
    printf("Â¦"); 
    for (i=0;i<pad;i++) putchar(' '); 
    printf("%s", title);
    for (i=0;i<60 - pad - (int)strlen(title); i++) putchar(' '); 
    printf("Â¦\n");
    printf("+"); 
    for (i=0;i<60;i++) putchar('-'); 
    printf("Â¦\n");
    for (i=0;i<nlines;i++) {
        printf("Â¦ "); 
        printf("%s", lines[i]);
        for (j=0;j<58 - (int)strlen(lines[i]); j++) putchar(' ');
        printf(" Â¦\n");
    }
    printf("+"); 
    for (i=0;i<60;i++) putchar('-'); 
    printf("+\n");
    printf(RESET);
}

void header_anim(const char *username) {
    clear_screen();
    printf(BOLD CYAN);
    type_print("==============================================", 1);
    type_print("           Welcome to Budget Tracker          ", 1);
    type_print("==============================================", 1);
    printf(RESET);
    printf(GREEN "Hello, %s\n\n" RESET, username);
    spinner(6);
}
// ---------------------- Add flows ----------------------

// check income duplicate for same category and month/year
int income_duplicate(TxnList *list, const char *category, const char *date) {
    int m = parse_month(date), y = parse_year(date);
    if (!m || !y) return 0;
    int i;
    for (i=0;i<list->size;i++) {
        if (strcmp(list->arr[i].type, "Income")==0 && 
            strcmp(list->arr[i].category, category)==0) {
            int im = parse_month(list->arr[i].date), iy = parse_year(list->arr[i].date);
            if (im == m && iy == y) return 1;
        }
    }
    return 0;
}

void add_income(TxnList *list) {
    Transaction t; 
    char buf[256];
    t.id = next_txn_id(list); 
    strncpy(t.type, "Income", sizeof(t.type)-1);
    printf("Enter income category (e.g., Salary): ");
    safe_input(buf, sizeof(buf)); 
    if (strlen(buf)==0) 
        strncpy(t.category, "Salary", sizeof(t.category)-1); 
    else 
        strncpy(t.category, buf, sizeof(t.category)-1);
    
    printf("Enter amount: "); 
    safe_input(buf, sizeof(buf)); 
    t.amount = atof(buf);
    
    printf("Enter date (DD/MM/YYYY) leave blank for today: "); 
    safe_input(buf, sizeof(buf));
    if (strlen(buf)==0) {
        time_t tt = time(NULL); 
        struct tm *tm = localtime(&tt);
        snprintf(t.date, sizeof(t.date), "%02d/%02d/%04d", tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900);
    } else {
        if (!date_valid(buf)) { 
            printf(RED "Invalid date. Aborted.\n" RESET); 
            return; 
        }
        strncpy(t.date, buf, sizeof(t.date)-1);
    }
    // duplicate check
    if (income_duplicate(list, t.category, t.date)) { 
        printf(RED "Income already added for this category in this month.\n" RESET); 
        return; 
    }
    
    printf("Enter note (optional): "); 
    safe_input(buf, sizeof(buf)); 
    strncpy(t.note, buf, sizeof(t.note)-1);
    push_txn(list, t); 
    printf(GREEN "Income added (ID %d)\n" RESET, t.id);
}

void add_expense(TxnList *list, char cats[][MAX_CAT_LEN], int *catcount) {
    Transaction t; 
    char buf[256];
    int i;
    t.id = next_txn_id(list); 
    strncpy(t.type, "Expense", sizeof(t.type)-1);
    printf("Choose category number or 0 for custom:\n");
    for (i=0;i<*catcount;i++) 
        printf(" %d. %s\n", i+1, cats[i]);
    printf(" 0. Custom\nChoice: "); 
    safe_input(buf, sizeof(buf));
    int ch = atoi(buf);
    if (ch==0) { 
        printf("Enter custom category name: "); 
        safe_input(buf, sizeof(buf)); 
        if (strlen(buf)==0) 
            strncpy(t.category,"Others",sizeof(t.category)-1); 
        else { 
            strncpy(t.category, buf, sizeof(t.category)-1); 
            if (*catcount < MAX_CATS) { 
                strncpy(cats[*catcount], buf, MAX_CAT_LEN-1); 
                (*catcount)++; 
            } 
        } 
    } else { 
        if (ch <1 || ch > *catcount) { 
            printf(RED "Invalid\n" RESET); 
            return; 
        } 
        strncpy(t.category, cats[ch-1], sizeof(t.category)-1); 
    }
    
    printf("Enter amount: "); 
    safe_input(buf, sizeof(buf)); 
    t.amount = atof(buf);
    
    printf("Enter date (DD/MM/YYYY) leave blank for today: "); 
    safe_input(buf, sizeof(buf));
    if (strlen(buf)==0) { 
        time_t tt = time(NULL); 
        struct tm *tm = localtime(&tt); 
        snprintf(t.date, sizeof(t.date), "%02d/%02d/%04d", tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900); 
    } else { 
        if (!date_valid(buf)) { 
            printf(RED "Invalid date.\n" RESET); 
            return; 
        } 
        strncpy(t.date, buf, sizeof(t.date)-1); 
    }
    
    printf("Enter note (optional): "); 
    safe_input(buf, sizeof(buf)); 
    strncpy(t.note, buf, sizeof(t.note)-1);
    push_txn(list, t); 
    printf(GREEN "Expense added (ID %d)\n" RESET, t.id);
}

// ---------------------- Show / edit / delete ----------------------

void show_transactions(TxnList *list) {
    if (list->size==0) { 
        printf("No transactions.\n"); 
        return; 
    }
    printf(BOLD "---------------------------------------------------------------\n" RESET);
    printf(BOLD " ID | DATE       | TYPE     | CATEGORY          | AMOUNT    | NOTE\n" RESET);
    printf(BOLD "---------------------------------------------------------------\n" RESET);
    double inc = 0, exp = 0;
    int i;
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i];
        printf("%3d | %-10s | %-8s | %-16s | %9.2f | %s\n", 
               t->id, t->date, t->type, t->category, t->amount, 
               strlen(t->note)?t->note:"NA");
        if (strcmp(t->type, "Income")==0) 
            inc += t->amount; 
        else 
            exp += t->amount;
    }
    printf(BOLD "---------------------------------------------------------------\n" RESET);
    printf("Total Income : Rs. %.2f\nTotal Expense: Rs. %.2f\nSavings      : Rs. %.2f\n", inc, exp, inc-exp);
}

Transaction* find_txn(TxnList *list, int id) {
	int i;
    for (i=0;i<list->size;i++) 
        if (list->arr[i].id == id) 
            return &list->arr[i];
    return NULL;
}

void edit_transaction(TxnList *list) {
    if (list->size==0) { 
        printf("No records.\n"); 
        return; 
    }
    char buf[128]; 
    printf("Enter transaction ID to edit: "); 
    safe_input(buf, sizeof(buf)); 
    int id = atoi(buf);
    Transaction *t = find_txn(list, id); 
    if (!t) { 
        printf(RED "Not found\n" RESET); 
        return; 
    }
    printf("Current: ID %d | %s | %s | %s | %.2f | %s\n", 
           t->id, t->date, t->type, t->category, t->amount, t->note);
    printf("Enter field to edit: 1-Type 2-Category 3-Amount 4-Date 5-Note 0-Cancel: "); 
    safe_input(buf, sizeof(buf));
    int ch = atoi(buf);
    if (ch==0) return;
    printf("Enter new value: "); 
    safe_input(buf, sizeof(buf));
    if (ch==1) 
        strncpy(t->type, buf, sizeof(t->type)-1);
    else if (ch==2) 
        strncpy(t->category, buf, sizeof(t->category)-1);
    else if (ch==3) 
        t->amount = atof(buf);
    else if (ch==4) { 
        if (!date_valid(buf)) { 
            printf(RED "Invalid date\n" RESET); 
            return; 
        } 
        strncpy(t->date, buf, sizeof(t->date)-1); 
    }
    else if (ch==5) 
        strncpy(t->note, buf, sizeof(t->note)-1);
    printf(GREEN "Updated.\n" RESET);
}

void delete_transaction(TxnList *list) {
    if (list->size==0) { 
        printf("No records.\n"); 
        return; 
    }
    char buf[128]; 
    printf("Enter transaction ID to delete: "); 
    safe_input(buf, sizeof(buf)); 
    int id = atoi(buf);
    int idx = -1; 
    int i,j;
    for (i=0;i<list->size;i++) 
        if (list->arr[i].id==id) { 
            idx=i; break; 
        }
    if (idx==-1) { 
        printf(RED "Not found\n" RESET); 
        return; 
    }
    for (j=idx;j<list->size-1;j++) 
        list->arr[j] = list->arr[j+1];
    list->size--;
    printf(GREEN "Deleted.\n" RESET);
}

// ---------------------- Manage Categories ----------------------

void manage_categories(char cats[][MAX_CAT_LEN], int *catcount) {
    int choice;
    char buf[256];
    
    do {
        printf("\n=== MANAGE CATEGORIES ===\n");
        printf("1. View Categories\n");
        printf("2. Add Category\n");
        printf("3. Back to Main Menu\n");
        printf("Choice: ");
        safe_input(buf, sizeof(buf));
        choice = atoi(buf);
		switch(choice) {
            case 1:
                printf("\n=== CURRENT CATEGORIES ===\n");
                int i;
                for ( i = 0; i < *catcount; i++) {
                    printf("%d. %s\n", i+1, cats[i]);
                }
                break;
                
            case 2:
                if (*catcount >= MAX_CATS) {
                    printf(RED "Category limit reached!\n" RESET);
                    break;
                }
                printf("Enter new category name: ");
                safe_input(buf, sizeof(buf));
                if (strlen(buf) > 0) {
                    // Check for duplicate
                    int i,duplicate = 0;
                    for (i = 0; i < *catcount; i++) {
                        if (strcmp(cats[i], buf) == 0) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (!duplicate) {
                        strncpy(cats[*catcount], buf, MAX_CAT_LEN-1);
                        (*catcount)++;
                        printf(GREEN "Category added successfully!\n" RESET);
                    } else {
                        printf(RED "Category already exists!\n" RESET);
                    }
                }
                break;
                
            case 3:
                return;
                
            default:
                printf(RED "Invalid choice!\n" RESET);
        }
    } while (choice != 3);
}

// ---------------------- Summaries & Health ----------------------

void financial_health(TxnList *list, int month, int year) {
    double income = 0, expense = 0;
    // category sums
    char cats[50][MAX_CAT_LEN]; 
    double vals[50]; 
    int ccount=0;
    int i,j;
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i];
        int m = parse_month(t->date), y = parse_year(t->date);
        if (m!=month || y!=year) continue;
        
        if (strcmp(t->type, "Income")==0) {
            income += t->amount;
        } else {
            expense += t->amount;
            int found = -1;
            for (j=0;j<ccount;j++) {
                if (strcmp(cats[j], t->category)==0) { 
                    found = j; 
                    break; 
                }
            }
            if (found == -1) { 
                if (ccount < 50) { 
                    strncpy(cats[ccount], t->category, MAX_CAT_LEN-1); 
                    vals[ccount] = t->amount; 
                    ccount++; 
                }
            } else {
                vals[found] += t->amount;
            }
        }
    }
    
    printf("\nFinancial overview: Income Rs. %.2f | Expense Rs. %.2f | Savings Rs. %.2f\n", 
           income, expense, income-expense);
    printf("Health: ");
    
    if (expense > income) { 
        printf(RED "Danger - expenses exceed income\n" RESET); 
        printf("Tip: Reduce discretionary spending, prioritize essential bills.\n"); 
    } else {
        double ratio = (income > 0) ? (expense/income*100.0) : 0;
        if (ratio > 80) { 
            printf(RED "Risk - high spending (%.1f%% of income)\n" RESET, ratio); 
            printf("Tip: Cut shopping/dining, track subscriptions.\n"); 
        } else if (ratio > 50) { 
            printf(YELLOW "Caution - moderate spending (%.1f%%)\n" RESET, ratio); 
            printf("Tip: Review recurring expenses.\n"); 
        } else { 
            printf(GREEN "Healthy (%.1f%%)\n" RESET, ratio); 
            printf("Tip: Maintain savings and consider goals.\n"); 
        }
    }
    
    if (ccount>0) {
        // sort top categories (bubble sort)
        int i,j;
        for (i=0;i<ccount-1;i++) {
            for (j=0;j<ccount-i-1;j++) {
                if (vals[j] < vals[j+1]) {
                    // swap values
                    double temp_val = vals[j];
                    vals[j] = vals[j+1];
                    vals[j+1] = temp_val;
                    
                    // swap categories
                    char temp_cat[MAX_CAT_LEN];
                    strcpy(temp_cat, cats[j]);
                    strcpy(cats[j], cats[j+1]);
                    strcpy(cats[j+1], temp_cat);
                }
            }
        }
        
        printf("\nTop spending categories:\n");
        int limit = (ccount < 3) ? ccount : 3;
        for (i=0; i < limit; i++) {
            printf("%d) %s - Rs. %.2f\n", i+1, cats[i], vals[i]);
        }
    }
}

void monthly_summary(TxnList *list) {
    char buf[64]; 
    printf("Enter month (1-12): "); 
    safe_input(buf, sizeof(buf)); 
    int month = atoi(buf);
    printf("Enter year (YYYY): "); 
    safe_input(buf, sizeof(buf)); 
    int year = atoi(buf);
    int i;
    double inc=0, exp=0;
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i];
        int m = parse_month(t->date), y = parse_year(t->date);
        if (m==month && y==year) { 
            if (strcmp(t->type,"Income")==0) 
                inc += t->amount; 
            else 
                exp += t->amount; 
        }
    }
    
    printf("\n====== Monthly Summary %02d/%04d ======\n", month, year);
    printf("Total Income : Rs. %.2f\nTotal Expense: Rs. %.2f\nSavings      : Rs. %.2f\n", inc, exp, inc-exp);
    financial_health(list, month, year);
}

void yearly_summary(TxnList *list) {
    char buf[64]; 
    printf("Enter year (YYYY): "); 
    safe_input(buf, sizeof(buf)); 
    int year = atoi(buf);
    int i;
    double months_income[13]={0}, months_expense[13]={0};
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i];
        int m = parse_month(t->date), y = parse_year(t->date);
        if (y==year && m>=1 && m<=12) {
            if (strcmp(t->type,"Income")==0) 
                months_income[m] += t->amount; 
            else 
                months_expense[m] += t->amount;
        }
    }
    int m;
    int count=0; 
    for (m=1;m<=12;m++) 
        if (months_income[m] || months_expense[m]) 
            count++;
            
    if (count < 12) 
        printf(YELLOW "Note: Data present for %d month(s). Add other months for full yearly summary.\n" RESET, count);
    
    double yi=0, ye=0; 
    for (m=1;m<=12;m++) { 
        yi += months_income[m]; 
        ye += months_expense[m]; 
    }
    
    printf("\n===== Yearly Summary %04d =====\n", year);
    printf("Total Income : Rs. %.2f\nTotal Expense: Rs. %.2f\nSavings      : Rs. %.2f\n", yi, ye, yi-ye);
}

// ---------------------- Report generation & export ----------------------

void generate_and_export_report(TxnList *list, const char *username) {
    double totInc=0, totExp=0; 
    int i;
    for (i=0;i<list->size;i++) { 
        if (strcmp(list->arr[i].type,"Income")==0) 
            totInc += list->arr[i].amount; 
        else 
            totExp += list->arr[i].amount; 
    }
    
    printf("\n===== PROFESSIONAL REPORT for %s =====\n", username);
    printf("Total Income: Rs. %.2f\nTotal Expense: Rs. %.2f\nNet Savings: Rs. %.2f\nTransactions count: %d\n", 
           totInc, totExp, totInc-totExp, list->size);
    printf("\nTransactions:\n");
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i];
        printf("ID:%d | %s | %s | %s | Rs. %.2f\n  Note: %s\n", 
               t->id, t->date, t->type, t->category, t->amount, 
               strlen(t->note)?t->note:"NA");
    }
    
    // Export to txt
    char fname[256]; 
    snprintf(fname, sizeof(fname), "report_%s.txt", username);
    FILE *f = fopen(fname, "w"); 
    if (!f) { 
        printf(RED "Failed to export.\n" RESET); 
        return; 
    }
    
    fprintf(f, "PROFESSIONAL REPORT for %s\n", username);
    fprintf(f, "Total Income: Rs. %.2f\nTotal Expense: Rs. %.2f\nNet Savings: Rs. %.2f\nTransactions count: %d\n\n", 
            totInc, totExp, totInc-totExp, list->size);
    
    for (i=0;i<list->size;i++) {
        Transaction *t = &list->arr[i];
        fprintf(f, "ID:%d | %s | %s | %s | Rs. %.2f\nNote: %s\n\n", 
                t->id, t->date, t->type, t->category, t->amount, 
                strlen(t->note)?t->note:"NA");
    }
    fclose(f);
    printf(GREEN "Report exported to %s\n" RESET, fname);
}

// ---------------------- Settings ----------------------

void about_info() {
    printf("\nAbout\nDevelopers: Mahandar Kumar and Tushar Kumar\nStudents, FAST-NUCES Karachi\nProject: Budget Tracker (Console)\n");
}

// ---------------------- Main user loop ----------------------

void user_session(const char *username) {
    TxnList list; 
    init_txnlist(&list); 
    load_transactions(username, &list);
    double budget_limit = load_budget_limit(username);
    char categories[MAX_CATS][MAX_CAT_LEN]; 
    int catcount = load_default_categories(categories);
    
    while (1) {
        header_anim(username);
        printf(BOLD YELLOW "\nMenu Options:\n" RESET);
        printf("1. Add Transaction\n2. View Transactions\n3. Manage Categories\n");
        printf("4. View Summary\n5. Edit Transaction\6. Delete Transaction\n");
        printf("7. Set Budget\n8. Generate & Export Report\n9. Settings\n");
        printf("10. Save & Logout\n0. Exit\n" RESET);
        
        printf("Enter choice: "); 
        char buf[64]; 
        safe_input(buf, sizeof(buf)); 
        int ch = atoi(buf);
        int i;
        if (ch==1) {
            printf("Add: 1-Income\n2-Expense\nOther to back: "); 
            safe_input(buf, sizeof(buf)); 
            int a = atoi(buf);
            if (a==1) { 
                add_income(&list); 
            } else if (a==2) { 
                add_expense(&list, categories, &catcount); 
                // Budget check
                if (budget_limit>0) { 
                    double inc=0, exp=0; 
                    for (i=0;i<list.size;i++){ 
                        if (strcmp(list.arr[i].type,"Income")==0) 
                            inc += list.arr[i].amount; 
                        else 
                            exp += list.arr[i].amount; 
                    } 
                    if (exp > budget_limit) 
                        printf(RED "Warning: Budget limit exceeded (%.2f)\n" RESET, budget_limit); 
                } 
            } else {
                printf("Cancelled\n");
            }
        } else if (ch==2) { 
            show_transactions(&list); 
        } else if (ch==3) { 
            manage_categories(categories, &catcount); 
        } else if (ch==4) { 
            printf("1. Monthly 2. Yearly (other cancel): "); 
            safe_input(buf, sizeof(buf)); 
            int s = atoi(buf); 
            if (s==1) 
                monthly_summary(&list); 
            else if (s==2) 
                yearly_summary(&list); 
            else 
                printf("Cancelled\n"); 
        } else if (ch==5) { 
            show_transactions(&list); 
            edit_transaction(&list); 
        } else if (ch==6) { 
            show_transactions(&list); 
            delete_transaction(&list); 
        } else if (ch==7) { 
            printf("Enter monthly budget limit (0 to disable): "); 
            safe_input(buf, sizeof(buf)); 
            budget_limit = atof(buf); 
            save_budget_limit(username, budget_limit); 
            printf(GREEN "Budget saved.\n" RESET); 
        } else if (ch==8) { 
            generate_and_export_report(&list, username); 
        } else if (ch==9) { 
            printf("Settings: 1-Change Password 2-About 3-Back: "); 
            safe_input(buf, sizeof(buf)); 
            int s = atoi(buf); 
            if (s==1) { 
                char oldp[128], newp[128]; 
                printf("Enter current password: "); 
                safe_input(oldp, sizeof(oldp)); 
                printf("Enter new password: "); 
                safe_input(newp, sizeof(newp)); 
                if (change_user_password(username, oldp, newp)) 
                    printf(GREEN "Password changed.\n" RESET); 
                else 
                    printf(RED "Change failed.\n" RESET); 
            } else if (s==2) 
                about_info(); 
        } else if (ch==10) { 
            if (save_transactions(username, &list)) 
                printf(GREEN "Saved.\n" RESET); 
            else 
                printf(RED "Save failed.\n" RESET); 
            free_txnlist(&list); 
            return; 
        } else if (ch==0) { 
            if (save_transactions(username, &list)) 
                printf(GREEN "Saved.\n" RESET); 
            else 
                printf(RED "Save failed.\n" RESET); 
            free_txnlist(&list); 
            printf("Exiting. Goodbye!\n"); 
            exit(0); 
        } else {
            printf("Invalid\n");
        }
        
        printf("Press Enter to continue..."); 
        char tmp[8]; 
        fgets(tmp, sizeof(tmp), stdin);
    }
}

// ---------------------- Auth menu ----------------------

void auth_menu() {
    while (1) {
        clear_screen();
        printf(BOLD CYAN "========================================\n" RESET);
        type_print("           Budget Tracker", 2);
        printf(BOLD CYAN "========================================\n" RESET);
        printf(YELLOW "1. Login\n2. Register\n3. Exit\n" RESET);
        printf("Choice: "); 
        char buf[128]; 
        safe_input(buf, sizeof(buf)); 
        int ch = atoi(buf);
        
        if (ch==1) {
            char u[64], p[128]; 
            printf("Username: "); 
            safe_input(u, sizeof(u)); 
            printf("Password: "); 
            safe_input(p, sizeof(p));
            spinner(6);
            if (verify_user(u, p)) { 
                printf(GREEN "Login successful.\n" RESET); 
                user_session(u); 
            } else {
                printf(RED "Login failed.\n" RESET);
            }
        } else if (ch==2) {
            char u[64], p[128]; 
            printf("Choose username: "); 
            safe_input(u, sizeof(u)); 
            if (strlen(u)==0) { 
                printf("Invalid.\n"); 
                continue; 
            }
            if (user_exists(u)) { 
                printf(RED "Taken.\n" RESET); 
                continue; 
            }
            printf("Choose password: "); 
            safe_input(p, sizeof(p));
            if (register_user(u, p)) 
                printf(GREEN "Registered. Login now.\n" RESET); 
            else 
                printf(RED "Register failed.\n" RESET);
        } else if (ch==3) { 
            printf("Goodbye.\n"); 
            exit(0); 
        } else {
            printf("Invalid.\n");
        }
        
        printf("Press Enter..."); 
        char tmp[8]; 
        fgets(tmp, sizeof(tmp), stdin);
    }
}

int main() {
    clear_screen();
    printf(BOLD MAGENTA);
    type_print("========================================", 1);
    type_print("*     Budget Tracker - Console App     *", 1);
    type_print("========================================", 1);
    printf(RESET);
    spinner(6);
    auth_menu();
    return 0;
}
