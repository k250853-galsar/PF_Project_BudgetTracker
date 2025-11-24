/*
  budget_tracker_optimized.c
  Optimized version with centered container and left-aligned menus.
  Compile: gcc -o budget_tracker budget_tracker_optimized.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* Cross-platform sleep utility */
#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

/* ---------- Config & constants ---------- */
#define TERM_WIDTH 80
#define CONTAINER_WIDTH 70  // Reduced width for the container
#define USERS_CSV "users.csv"
#define MAX_TXNS 2000
#define MAX_CATS 60
#define MAX_LINE 1024
#define XOR_KEY 0x5A

/* ANSI colors - Standardized */
#define C_RESET  "\033[0m"
#define C_BOLD   "\033[1m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_RED    "\033[31m"
#define C_CYAN   "\033[36m"
#define C_MAGENTA "\033[35m"
#define C_B_GREEN "\033[1;32m"
#define C_B_RED   "\033[1;31m"
#define C_BLUE    "\033[34m"
#define C_B_BLUE  "\033[1;34m"

/* ---------- Types ---------- */
typedef struct {
    int day, month, year;
    char type[12];      /* "Income" / "Expense" */
    char category[64];
    double amount;
    char note[192];
    int id;
} Transaction;

/* ---------- Global session ---------- */
static char cur_user[64] = "";
static Transaction txns[MAX_TXNS];
static int txn_count = 0;
static char cats[MAX_CATS][64];
static int cat_count = 0;
static double monthly_budget = 0.0;

/* ---------- Utility functions: UI/UX ---------- */

// Print container border
static void print_border_line(int is_top) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s", pad, "", C_B_BLUE);
    if (is_top) {
        printf("+");
        for (int i = 0; i < CONTAINER_WIDTH - 2; i++) printf("-");
        printf("+");
    } else {
        printf("+");
        for (int i = 0; i < CONTAINER_WIDTH - 2; i++) printf("-");
        printf("+");
    }
    printf("%s\n", C_RESET);
}

static void print_side_borders(void) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s¦%*s¦%s\n", pad, "", C_B_BLUE, CONTAINER_WIDTH - 2, "", C_RESET);
}

// Centered text within container
static void print_centered_in_container(const char *s, const char *color) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    int text_pad = (CONTAINER_WIDTH - (int)strlen(s)) / 2;
    if (text_pad < 0) text_pad = 0;
    printf("%*s%s¦%*s%s%s%*s¦%s\n", 
           pad, "", C_B_BLUE, 
           text_pad, "", color, s, 
           CONTAINER_WIDTH - 2 - text_pad - (int)strlen(s), "", 
           C_RESET);
}

// Left-aligned text within container
static void print_left_in_container(const char *s, const char *color) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s¦ %s%s%*s¦%s\n", 
           pad, "", C_B_BLUE, 
           color, s, 
           CONTAINER_WIDTH - 3 - (int)strlen(s), "", 
           C_RESET);
}

// Empty line in container
static void print_empty_line_in_container(void) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s¦%*s¦%s\n", pad, "", C_B_BLUE, CONTAINER_WIDTH - 2, "", C_RESET);
}

// Separator line in container
static void print_separator_in_container(void) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s¦", pad, "", C_B_BLUE);
    for (int i = 0; i < CONTAINER_WIDTH - 2; i++) printf("-");
    printf("¦%s\n", C_RESET);
}

static void print_header(const char *title) {
    system("clear || cls");
    print_border_line(1);  // Top border
    print_empty_line_in_container();
    print_centered_in_container("==============================================", C_MAGENTA);
    print_centered_in_container(title, C_BOLD C_MAGENTA);
    print_centered_in_container("==============================================", C_MAGENTA);
    print_empty_line_in_container();
}

static void print_footer(void) {
    print_empty_line_in_container();
    print_border_line(0);  // Bottom border
}

static void wait_enter_center(void) {
    print_centered_in_container("(Press Enter to continue)", C_CYAN);
    if (getchar() != '\n') while (getchar() != '\n');
}

static void print_error(const char* msg) {
    print_centered_in_container(msg, C_B_RED);
}

static void print_success(const char* msg) {
    print_centered_in_container(msg, C_B_GREEN);
}

static void get_input(const char *prompt, char *out, int sz) {
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s¦ %s: ", pad, "", C_B_BLUE, prompt);
    printf("%s", C_RESET);
    fflush(stdout);
    if (!fgets(out, sz, stdin)) { out[0] = '\0'; return; }
    out[strcspn(out, "\n")] = 0;
}

/* ---------- Utility functions: Core Logic ---------- */

static void xor_str(char *s) {
    for (int i = 0; s[i]; ++i) s[i] ^= XOR_KEY;
}

static int next_txn_id(void) {
    int m = 0;
    for (int i = 0; i < txn_count; ++i) if (txns[i].id > m) m = txns[i].id;
    return m + 1;
}

static void txns_path(const char *user, char *out, int sz) {
    snprintf(out, sz, "user_%s_txns.csv", user);
}

static void settings_path(const char *user, char *out, int sz) {
    snprintf(out, sz, "user_%s_settings.txt", user);
}

static int is_valid_date(const char *d) {
    int dd, mm, yy;
    if (sscanf(d, "%d/%d/%d", &dd, &mm, &yy) != 3) return 0;
    if (mm < 1 || mm > 12) return 0;
    if (dd < 1 || dd > 31) return 0;
    if (yy < 1900 || yy > 9999) return 0;
    return 1;
}

static void load_default_categories(void) {
    const char *d[] = {"Salary","Business","Other Income","Grocery","Utilities","Transport","Dining & Food","Shopping","Healthcare","Others"};
    cat_count = 0;
    for (size_t i = 0; i < sizeof(d)/sizeof(d[0]) && cat_count < MAX_CATS; ++i) {
        strncpy(cats[cat_count++], d[i], sizeof(cats[0])-1);
        cats[cat_count-1][sizeof(cats[0])-1] = '\0';
    }
}

static void add_category_session(const char *name) {
    if (cat_count < MAX_CATS) {
        strncpy(cats[cat_count++], name, sizeof(cats[0])-1);
        cats[cat_count-1][sizeof(cats[0])-1] = '\0';
    }
}

/* ---------- UI: Animation/UX Improvement (Loading Bar) ---------- */

static void welcome_animation(const char *username_display) {
    print_header("Personal Budget Tracker");
    if (username_display && username_display[0]) {
        char hello[128]; snprintf(hello, sizeof(hello), "Hello, %s", username_display);
        print_centered_in_container(hello, C_CYAN);
    } else {
        print_centered_in_container("Hello, User", C_CYAN);
    }
    
    // Loading Bar Animation
    print_empty_line_in_container();
    int bar_width = 40;
    int pad = (TERM_WIDTH - CONTAINER_WIDTH) / 2;
    printf("%*s%s¦", pad, "", C_B_BLUE);
    for (int i = 0; i <= bar_width; ++i) {
        printf("\r%*s%s¦%*s%s[", pad, "", C_B_BLUE, (CONTAINER_WIDTH - bar_width - 4) / 2, "", C_CYAN);
        for (int j = 0; j < i; ++j) printf("#");
        for (int j = i; j < bar_width; ++j) printf("-");
        printf("]%s", C_RESET);
        fflush(stdout);
        sleep_ms(30);
    }
    printf("%*s%s¦%s\n", CONTAINER_WIDTH - bar_width - 6, "", C_B_BLUE, C_RESET);
    sleep_ms(150);
    print_footer();
}

/* ---------- File I/O (Authentication, Transactions, Settings) ---------- */

static int verify_user_file(const char *username, const char *password) {
    FILE *f = fopen(USERS_CSV, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        char u[64], penc[128];
        if (sscanf(line, "%63[^,],%127[^\n]", u, penc) == 2) {
            if (strcmp(u, username) == 0) {
                char dec[128]; strncpy(dec, penc, sizeof(dec)-1); dec[sizeof(dec)-1]='\0'; xor_str(dec);
                if (strcmp(dec, password) == 0) ok = 1;
                break;
            }
        }
    }
    fclose(f);
    return ok;
}

static int user_exists(const char *username) {
    FILE *f = fopen(USERS_CSV, "r");
    if (!f) return 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char u[64];
        if (sscanf(line, "%63[^,],%*[^\n]", u) == 1) {
            if (strcmp(u, username) == 0) { fclose(f); return 1; }
        }
    }
    fclose(f); return 0;
}

static int save_transactions_for_user(const char *username) {
    char path[MAX_LINE]; txns_path(username, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < txn_count; ++i) {
        Transaction *t = &txns[i];
        // Ensure note does not contain commas by replacing with semi-colons (maintains CSV integrity)
        char safe_note[192]; strncpy(safe_note, t->note, sizeof(safe_note)-1); safe_note[sizeof(safe_note)-1]='\0';
        for (int j=0; safe_note[j]; ++j) if (safe_note[j] == ',') safe_note[j] = ';';
        fprintf(f, "%d,%s,%s,%.2f,%02d/%02d/%04d,%s\n", 
                t->id, t->type, t->category, t->amount, t->day, t->month, t->year, safe_note);
    }
    fclose(f); return 1;
}

static void load_transactions_for_user(const char *username) {
    txn_count = 0;
    char path[MAX_LINE]; txns_path(username, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && txn_count < MAX_TXNS) {
        Transaction t; memset(&t, 0, sizeof(t));
        if (sscanf(line, "%d,%11[^,],%63[^,],%lf,%d/%d/%d,%191[^\n]", 
                   &t.id, t.type, t.category, &t.amount, 
                   &t.day, &t.month, &t.year, t.note) == 8) {
            txns[txn_count++] = t;
        }
    }
    fclose(f);
}

static void load_settings_for_user(const char *username) {
    monthly_budget = 0.0;
    char path[MAX_LINE]; settings_path(username, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "budget:%lf", &monthly_budget) == 1) break;
    }
    fclose(f);
}

static void save_settings_for_user(const char *username) {
    char path[MAX_LINE]; settings_path(username, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "budget:%.2f\n", monthly_budget);
    fclose(f);
}

/* ---------- Business rules (Calculations) ---------- */

static double sum_income_month(int m, int y) {
    double s = 0.0;
    for (int i = 0; i < txn_count; ++i)
        if (strcmp(txns[i].type, "Income") == 0 && txns[i].month == m && txns[i].year == y)
            s += txns[i].amount;
    return s;
}

static double sum_expense_month(int m, int y) {
    double s = 0.0;
    for (int i = 0; i < txn_count; ++i)
        if (strcmp(txns[i].type, "Expense") == 0 && txns[i].month == m && txns[i].year == y)
            s += txns[i].amount;
    return s;
}

static int salary_exists_in_month(int m, int y) {
    for (int i = 0; i < txn_count; ++i)
        if (strcmp(txns[i].type, "Income") == 0 && strcmp(txns[i].category, "Salary") == 0 && txns[i].month == m && txns[i].year == y)
            return 1;
    return 0;
}

static Transaction* find_txn_by_id(int id) {
    for (int i = 0; i < txn_count; ++i) if (txns[i].id == id) return &txns[i];
    return NULL;
}

static int delete_txn_by_id(int id) {
    int idx = -1;
    for (int i = 0; i < txn_count; ++i) if (txns[i].id == id) { idx = i; break; }
    if (idx == -1) return 0;
    for (int j = idx; j < txn_count - 1; ++j) txns[j] = txns[j + 1];
    txn_count--;
    return 1;
}

static void get_transaction_details(Transaction *t) {
    char tmp[64];
    get_input("Enter amount", tmp, sizeof(tmp)); 
    t->amount = atof(tmp);
    if (t->amount <= 0) { print_error("Invalid amount."); return; }
    get_input("Enter note (optional)", t->note, sizeof(t->note));
}

/* ---------- Add Transaction Flow ---------- */

void add_transaction_flow_with_month(int m_pref, int y_pref) {
    if (txn_count >= MAX_TXNS) { print_error("Transaction limit reached."); wait_enter_center(); return; }

    while (1) {
        print_header("ADD TRANSACTION");
        print_left_in_container("1) Add Income", C_RESET);
        print_left_in_container("2) Add Expense", C_RESET);
        print_left_in_container("0) Back", C_RESET);
        print_empty_line_in_container();
        print_left_in_container("Enter choice:", C_RESET);
        char ch[16]; get_input("Choice", ch, sizeof(ch));
        if (ch[0] == '0') { print_footer(); return; }

        Transaction t; memset(&t,0,sizeof(t)); t.id = next_txn_id();
        int is_income = (ch[0] == '1');
        strncpy(t.type, is_income ? "Income" : "Expense", sizeof(t.type)-1);
        t.type[sizeof(t.type)-1] = '\0';

        // Date selection
        char datebuf[16], tmp[64];
        if (m_pref && y_pref) {
            get_input("Enter day of month (1-31) or empty for 1", tmp, sizeof(tmp));
            int dd = tmp[0] ? atoi(tmp) : 1;
            if (dd < 1 || dd > 31) dd = 1;
            snprintf(datebuf, sizeof(datebuf), "%02d/%02d/%04d", dd, m_pref, y_pref);
        } else {
            get_input("Enter date (DD/MM/YYYY) or empty for today", tmp, sizeof(tmp));
            if (tmp[0] == 0) { 
                time_t time_now = time(NULL); struct tm *tm = localtime(&time_now);
                snprintf(datebuf, sizeof(datebuf), "%02d/%02d/%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
            } else if (!is_valid_date(tmp)) { 
                print_error("Invalid date."); wait_enter_center(); continue; 
            } else { strncpy(datebuf, tmp, sizeof(datebuf)-1); datebuf[sizeof(datebuf)-1] = '\0'; }
        }
        if (sscanf(datebuf,"%d/%d/%d",&t.day,&t.month,&t.year) != 3) { print_error("Date processing error."); wait_enter_center(); continue; }

        // Category selection
        print_centered_in_container(is_income ? "Choose income category:" : "Choose expense category:", C_RESET);
        int sel_idxs[MAX_CATS], sel_count=0;
        for (int i=0; i<cat_count; i++) {
            int is_inc_cat = (strcasecmp(cats[i], "Salary")==0 || strcasecmp(cats[i], "Business")==0 || strcasecmp(cats[i], "Other Income")==0);
            if (is_income == is_inc_cat) {
                char b[80]; snprintf(b,sizeof(b), "%d) %s", sel_count+1, cats[i]); print_left_in_container(b, C_RESET);
                sel_idxs[sel_count++] = i;
            }
        }
        print_left_in_container("0) Custom", C_RESET);
        get_input("Enter choice number", tmp, sizeof(tmp));
        int sel_idx = atoi(tmp);

        if (sel_idx == 0) {
            get_input(is_income ? "Enter custom income category" : "Enter custom expense category", t.category, sizeof(t.category));
            if (t.category[0]) add_category_session(t.category);
        } else if (sel_idx > 0 && sel_idx <= sel_count) {
            strncpy(t.category, cats[sel_idxs[sel_idx-1]], sizeof(t.category)-1);
            t.category[sizeof(t.category)-1] = '\0';
        } else {
            print_error("Invalid selection."); wait_enter_center(); continue;
        }

        get_transaction_details(&t);
        if (t.amount <= 0) continue;

        // Validation checks
        if (is_income && strcasecmp(t.category, "Salary") == 0 && salary_exists_in_month(t.month, t.year)) {
            print_error("Salary already added for this month. Cannot add another.");
            wait_enter_center(); continue;
        }
        if (!is_income) {
            double inc = sum_income_month(t.month, t.year);
            if (inc <= 0.0) { print_error("Cannot add expense: no income recorded for this month."); wait_enter_center(); continue; }
            double ex_before = sum_expense_month(t.month, t.year);
            if (ex_before + t.amount > inc) {
                print_centered_in_container("Warning: Expense exceeds income for the month.", C_YELLOW);
                get_input("Proceed? (Y/N)", tmp, sizeof(tmp));
                if (!(tmp[0]=='Y' || tmp[0]=='y')) { print_error("Cancelled."); wait_enter_center(); continue; }
            }
            if (monthly_budget > 0.0 && ex_before + t.amount > monthly_budget) {
                print_centered_in_container("Alert: Expense crosses monthly budget!", C_B_RED);
            }
        }

        txns[txn_count++] = t;
        save_transactions_for_user(cur_user);
        print_success(is_income ? "Income added successfully." : "Expense added successfully.");
        wait_enter_center();
        break; 
    }
    print_footer();
}

/* ---------- Authentication Menu (Entry point) ---------- */
static void auth_menu(void) {
    while (1) {
        print_header("AUTHENTICATION");
        print_left_in_container("1) Login", C_RESET);
        print_left_in_container("2) Register", C_RESET);
        print_left_in_container("3) Exit", C_RESET);
        print_empty_line_in_container();
        print_left_in_container("Enter choice (1-3):", C_RESET);
        
        char buf[32], u[64], p[128]; 
        get_input("Choice", buf, sizeof(buf));
        
        if (buf[0] == '1') {
            print_header("LOGIN");
            get_input("Enter username", u, sizeof(u));
            get_input("Enter password", p, sizeof(p));
            welcome_animation(NULL);
            if (verify_user_file(u, p)) {
                strncpy(cur_user, u, sizeof(cur_user)-1); cur_user[sizeof(cur_user)-1] = '\0';
                load_default_categories();
                load_transactions_for_user(cur_user);
                load_settings_for_user(cur_user);
                welcome_animation(cur_user);
                return;
            } else {
                print_error("Login failed. Invalid credentials.");
                wait_enter_center();
            }
        } else if (buf[0] == '2') {
            print_header("REGISTER");
            get_input("Choose a username", u, sizeof(u));
            if (user_exists(u)) { print_error("Username already exists."); wait_enter_center(); continue; }
            get_input("Choose a password", p, sizeof(p));
            if (u[0] && p[0]) {
                char enc[128]; strncpy(enc, p, sizeof(enc)-1); enc[sizeof(enc)-1] = '\0'; xor_str(enc);
                FILE *f = fopen(USERS_CSV, "a");
                if (f) {
                    fprintf(f, "%s,%s\n", u, enc); fclose(f);
                    char path[MAX_LINE];
                    txns_path(u, path, sizeof(path));
                    FILE *g = fopen(path, "w"); if (g) fclose(g);
                    settings_path(u, path, sizeof(path));
                    g = fopen(path, "w"); if (g) { fprintf(g, "budget:0.00\n"); fclose(g); }
                    print_success("Registered successfully. Please login.");
                } else { print_error("Registration failed (file error)."); }
            } else { print_error("Username and password cannot be empty."); }
            wait_enter_center();
        } else if (buf[0] == '3' || buf[0] == '0') {
            print_header("Goodbye."); exit(0);
        } else {
            print_error("Invalid choice."); wait_enter_center();
        }
        print_footer();
    }
}

/* ---------- Dashboard Menu ---------- */
void dashboard_menu(void) {
    while (1) {
        print_header("DASHBOARD");
        print_left_in_container("1) View Month Summary", C_RESET);
        print_left_in_container("2) Add Transaction (for specific month)", C_RESET);
        print_left_in_container("3) View Recent Transactions", C_RESET);
        print_left_in_container("0) Back to Main Menu", C_RESET);
        print_empty_line_in_container();
        print_left_in_container("Enter choice:", C_RESET);
        
        char buf[32]; get_input("Choice", buf, sizeof(buf));
        
        if (buf[0] == '0') { print_footer(); return; }
        
        if (buf[0] == '1' || buf[0] == '2') {
            char m_str[16], y_str[16];
            get_input("Enter month (1-12)", m_str, sizeof(m_str));
            get_input("Enter year (e.g., 2025)", y_str, sizeof(y_str));
            int m = atoi(m_str), y = atoi(y_str);
            
            if (m < 1 || m > 12 || y < 1900 || y > 9999) {
                print_error("Invalid month or year."); wait_enter_center(); continue;
            }

            if (buf[0] == '2') {
                add_transaction_flow_with_month(m, y);
            } else {
                double inc = sum_income_month(m, y);
                double ex = sum_expense_month(m, y);
                double net = inc - ex;
                
                char h_buf[128]; snprintf(h_buf, sizeof(h_buf), "Dashboard - %02d/%04d", m, y);
                print_header(h_buf);
                
                char line[128];
                snprintf(line, sizeof(line), "Income:   Rs. %10.2f", inc);
                print_centered_in_container(line, C_B_GREEN);
                snprintf(line, sizeof(line), "Expenses: Rs. %10.2f", ex);
                print_centered_in_container(line, C_B_RED);
                snprintf(line, sizeof(line), "Net:      Rs. %10.2f", net);
                print_centered_in_container(line, C_YELLOW);
                print_separator_in_container();

                if (monthly_budget > 0.0) {
                    snprintf(line, sizeof(line), "Monthly Budget: Rs. %.2f", monthly_budget);
                    print_centered_in_container(line, C_RESET);
                    if (ex > monthly_budget) {
                        print_error("Alert: Expenses exceed monthly budget!");
                    } else if (ex > monthly_budget * 0.8) {
                        print_centered_in_container("Warning: Expenses approaching budget limit", C_YELLOW);
                    } else {
                        print_success("Within budget limits");
                    }
                }
                
                wait_enter_center();
            }
        }
        else if (buf[0] == '3') {
            print_header("RECENT TRANSACTIONS");
            if (txn_count == 0) {
                print_centered_in_container("No transactions recorded.", C_RESET);
            } else {
                int count = (txn_count < 10) ? txn_count : 10;
                for (int i = txn_count - count; i < txn_count; ++i) {
                    Transaction *t = &txns[i];
                    char line[256];
                    char* color = (strcmp(t->type, "Income") == 0) ? C_GREEN : C_RED;
                    snprintf(line, sizeof(line), "ID:%d | %02d/%02d/%04d | %-8s | %-15s | %.2f | %s",
                            t->id, t->day, t->month, t->year, 
                            t->type, t->category, t->amount, 
                            t->note[0] ? t->note : "NA");
                    print_left_in_container(line, color);
                }
                char count_msg[64];
                snprintf(count_msg, sizeof(count_msg), "Showing %d most recent transactions", count);
                print_empty_line_in_container();
                print_centered_in_container(count_msg, C_RESET);
            }
            wait_enter_center();
        }
        else {
            print_error("Invalid choice.");
            wait_enter_center();
        }
        print_footer();
    }
}

/* ---------- Main Menu (Entry point after Login) ---------- */
int main(void) {
    load_default_categories();
    auth_menu();
    while (cur_user[0]) {
        print_header("MAIN MENU");
        char greet[80]; snprintf(greet,sizeof(greet),"Hello, %s", cur_user); 
        print_centered_in_container(greet, C_CYAN);
        print_empty_line_in_container();
        print_left_in_container("1) Dashboard (Summary & Recent)", C_RESET);
        print_left_in_container("2) Add Transaction (Quick)", C_RESET);
        print_left_in_container("3) Manage Transactions (Edit/Delete/Search)", C_RESET);
        print_left_in_container("4) Manage Categories (View/Add)", C_RESET);
        print_left_in_container("5) View Detailed Summary (Monthly/Yearly)", C_RESET);
        print_left_in_container("6) Set Monthly Budget", C_RESET);
        print_left_in_container("7) Export Report (TXT)", C_RESET);
        print_left_in_container("8) Settings (Password/About)", C_RESET);
        print_left_in_container("9) Save & Logout", C_RESET);
        print_left_in_container("0) Exit", C_RESET);
        print_empty_line_in_container();
        
        char buf[32]; get_input("Enter choice", buf, sizeof(buf));
        
        if (buf[0]=='0') { 
            save_transactions_for_user(cur_user); 
            save_settings_for_user(cur_user); 
            print_header("Goodbye."); 
            break; 
        } else if (strcmp(buf,"1")==0) { dashboard_menu(); }
        else if (strcmp(buf,"2")==0) add_transaction_flow_with_month(0,0);
        else if (strcmp(buf,"3")==0) manage_transactions_menu();
        else if (strcmp(buf,"4")==0) manage_categories_menu();
        else if (strcmp(buf,"5")==0) view_summary_menu();
        else if (strcmp(buf,"6")==0) set_budget_menu();
        else if (strcmp(buf,"7")==0) generate_export_report();
        else if (strcmp(buf,"8")==0) settings_menu();
        else if (strcmp(buf,"9")==0) { 
            save_transactions_for_user(cur_user); 
            save_settings_for_user(cur_user); 
            cur_user[0]=0; 
            auth_menu(); 
        } else { print_error("Invalid choice."); wait_enter_center(); }
        print_footer();
    }
    return 0;
}

/* --- Remaining Menu Implementations --- */

void manage_transactions_menu(void) {
    while (1) {
        print_header("MANAGE TRANSACTIONS");
        print_left_in_container("1) Add Transaction (Quick)", C_RESET);
        print_left_in_container("2) Edit Transaction by ID", C_RESET);
        print_left_in_container("3) Delete Transaction by ID", C_RESET);
        print_left_in_container("4) Search Transactions by Date (DD/MM/YYYY)", C_RESET);
        print_left_in_container("0) Back", C_RESET);
        char buf[32]; get_input("Choice", buf, sizeof(buf));
        if (buf[0] == '0') { print_footer(); return; }
        if (buf[0] == '1') { add_transaction_flow_with_month(0,0); }
        else if (buf[0] == '2') {
            get_input("Enter transaction ID to edit", buf, sizeof(buf)); int id = atoi(buf);
            Transaction *t = find_txn_by_id(id);
            if (!t) { print_error("Not found."); wait_enter_center(); continue; }
            print_header("EDIT TRANSACTION");
            char tmp[128];
            snprintf(tmp,sizeof(tmp),"Current Type: %s", t->type); print_centered_in_container(tmp, C_RESET);
            get_input("Enter new type (Income/Expense) or blank", tmp, sizeof(tmp)); if (tmp[0]) { strncpy(t->type,tmp,sizeof(t->type)-1); t->type[sizeof(t->type)-1]='\0'; }
            snprintf(tmp,sizeof(tmp),"Current Category: %s", t->category); print_centered_in_container(tmp, C_RESET);
            get_input("Enter new category or blank", tmp, sizeof(tmp)); if (tmp[0]) { strncpy(t->category,tmp,sizeof(t->category)-1); t->category[sizeof(t->category)-1]='\0'; }
            snprintf(tmp,sizeof(tmp),"Current amount: %.2f", t->amount); print_centered_in_container(tmp, C_RESET);
            get_input("Enter new amount or blank", tmp, sizeof(tmp)); if (tmp[0]) t->amount = atof(tmp);
            char datebuf[16]; snprintf(datebuf,sizeof(datebuf), "%02d/%02d/%04d", t->day, t->month, t->year);
            snprintf(tmp,sizeof(tmp),"Current date: %s", datebuf); print_centered_in_container(tmp, C_RESET);
            get_input("Enter new date DD/MM/YYYY or blank", tmp, sizeof(tmp));
            if (tmp[0]) { 
                if (is_valid_date(tmp)) { 
                    int dd,mm,yy; 
                    if (sscanf(tmp,"%d/%d/%d",&dd,&mm,&yy) == 3) { t->day=dd; t->month=mm; t->year=yy; }
                } else print_error("Invalid date ignored."); 
            }
            snprintf(tmp,sizeof(tmp),"Current note: %s", t->note[0]?t->note:"NA"); print_centered_in_container(tmp, C_RESET);
            get_input("Enter new note or blank", tmp, sizeof(tmp)); if (tmp[0]) { strncpy(t->note,tmp,sizeof(t->note)-1); t->note[sizeof(t->note)-1]='\0'; }
            save_transactions_for_user(cur_user);
            print_success("Updated."); wait_enter_center();
        } else if (buf[0] == '3') {
            get_input("Enter transaction ID to delete", buf, sizeof(buf)); int id = atoi(buf);
            if (delete_txn_by_id(id)) { save_transactions_for_user(cur_user); print_success("Deleted."); }
            else print_error("Not found.");
            wait_enter_center();
        } else if (buf[0] == '4') {
            get_input("Enter date DD/MM/YYYY to search", buf, sizeof(buf));
            if (!is_valid_date(buf)) { print_error("Invalid date format."); wait_enter_center(); continue; }
            char h_buf[128]; snprintf(h_buf, sizeof(h_buf), "Search results for %s", buf);
            print_header(h_buf);
            int found = 0;
            int dd,mm,yy; sscanf(buf,"%d/%d/%d",&dd,&mm,&yy);
            for (int i=0;i<txn_count;i++) {
                if (txns[i].day==dd && txns[i].month==mm && txns[i].year==yy) {
                    char line[256]; char* color = (strcmp(txns[i].type, "Income") == 0) ? C_GREEN : C_RED;
                    snprintf(line,sizeof(line),"ID:%d | %02d/%02d/%04d | %-8s | %-15s | %.2f | %s",
                                               txns[i].id, txns[i].day, txns[i].month, txns[i].year, txns[i].type, txns[i].category, txns[i].amount, txns[i].note[0]?txns[i].note:"NA");
                    print_left_in_container(line, color); found++;
                }
            }
            if (!found) print_centered_in_container("No transactions found for that date.", C_RESET);
            wait_enter_center();
        } else { print_error("Invalid choice."); wait_enter_center(); }
        print_footer();
    }
}

void manage_categories_menu(void) {
    while (1) {
        print_header("MANAGE CATEGORIES");
        print_left_in_container("1) View categories", C_RESET);
        print_left_in_container("2) Add category (session)", C_RESET);
        print_left_in_container("0) Back", C_RESET);
        char c[64]; get_input("Choice", c, sizeof(c));
        if (c[0] == '0') { print_footer(); return; }
        if (c[0] == '1') {
            print_header("CATEGORIES");
            for (int i=0;i<cat_count;i++) { char line[128]; snprintf(line,sizeof(line), "%d) %s", i+1, cats[i]); print_left_in_container(line, C_RESET); }
            wait_enter_center();
        } else if (c[0] == '2') {
            char name[64]; get_input("Enter new category name", name, sizeof(name));
            add_category_session(name);
            print_success("Added (session).");
            wait_enter_center();
        } else { print_error("Invalid choice."); wait_enter_center(); }
        print_footer();
    }
}

void view_summary_menu(void) {
    while (1) {
        print_header("SUMMARY");
        print_left_in_container("1) Monthly summary", C_RESET);
        print_left_in_container("2) Yearly summary", C_RESET);
        print_left_in_container("0) Back", C_RESET);
        char c[64]; get_input("Choice", c, sizeof(c));
        if (c[0] == '0') { print_footer(); return; }
        if (c[0] == '1') {
            get_input("Enter month (1-12)", c, sizeof(c)); int m = atoi(c);
            get_input("Enter year (e.g., 2025)", c, sizeof(c)); int y = atoi(c);
            double inc = sum_income_month(m,y), ex = sum_expense_month(m,y);
            double net = inc - ex;
            char h_buf[128]; snprintf(h_buf, sizeof(h_buf), "Monthly Summary %02d/%04d", m, y);
            print_header(h_buf);
            char l1[80], l2[80], l3[80];
            snprintf(l1,sizeof(l1),"Total Income : Rs. %.2f", inc); print_centered_in_container(l1, C_B_GREEN);
            snprintf(l2,sizeof(l2),"Total Expense: Rs. %.2f", ex); print_centered_in_container(l2, C_B_RED);
            snprintf(l3,sizeof(l3),"Net Savings  : Rs. %.2f", net); print_centered_in_container(l3, C_YELLOW);

            print_centered_in_container("--- Financial Health ---", C_RESET);
            if (ex > inc) { print_error("Health: Expenses exceed income!"); } 
            else {
                double ratio = inc ? (ex/inc*100.0) : 0;
                if (ratio > 80.0) { print_centered_in_container("Health: High spending (>80% of income)", C_YELLOW); } 
                else { print_success("Health: Good"); }
            }
            wait_enter_center();
        } else if (c[0] == '2') {
            get_input("Enter year (e.g., 2025)", c, sizeof(c)); int y = atoi(c);
            double yi=0, ye=0; int months_present=0;
            for (int m=1;m<=12;m++) {
                double inc = sum_income_month(m,y), ex = sum_expense_month(m,y);
                if (inc || ex) months_present++;
                yi+=inc; ye+=ex;
            }
            char h_buf[128]; snprintf(h_buf, sizeof(h_buf), "Yearly Summary %04d", y);
            print_header(h_buf);
            char l1[80], l2[80], l3[80];
            snprintf(l1,sizeof(l1),"Year Income : Rs. %.2f", yi); print_centered_in_container(l1, C_B_GREEN);
            snprintf(l2,sizeof(l2),"Year Expense: Rs. %.2f", ye); print_centered_in_container(l2, C_B_RED);
            snprintf(l3,sizeof(l3),"Year Savings: Rs. %.2f", yi-ye); print_centered_in_container(l3, C_YELLOW);

            if (months_present < 12) {
                char tmp[128]; snprintf(tmp,sizeof(tmp),"Note: data present for %d month(s).", months_present);
                print_centered_in_container(tmp, C_RESET);
            }
            wait_enter_center();
        } else { print_error("Invalid choice."); wait_enter_center(); }
        print_footer();
    }
}

void set_budget_menu(void) {
    print_header("SET BUDGET");
    char b[64]; get_input("Enter monthly budget amount (0 to disable)", b, sizeof(b));
    monthly_budget = atof(b);
    save_settings_for_user(cur_user);
    print_success("Budget saved.");
    wait_enter_center();
    print_footer();
}

void generate_export_report(void) {
    print_header("GENERATE & EXPORT REPORT");
    char buf[32]; 
    get_input("Enter month (1-12)", buf, sizeof(buf)); 
    int m = atoi(buf);
    get_input("Enter year (YYYY)", buf, sizeof(buf)); 
    int y = atoi(buf);

    if (m < 1 || m > 12 || y < 1900 || y > 9999) {
        print_error("Invalid month or year. Aborting.");
        wait_enter_center(); 
        print_footer();
        return;
    }

    // Change file extension to .txt
    char fname[128]; snprintf(fname,sizeof(fname),"report_%s_%02d-%04d.txt", cur_user, m, y);
    FILE *f = fopen(fname, "w");
    if (!f) { print_error("Failed to create report file."); wait_enter_center(); print_footer(); return; }

    // Write formatted TXT content
    fprintf(f, "================================================================================\n");
    fprintf(f, "                      FINANCIAL REPORT: %02d/%04d\n", m, y);
    fprintf(f, "                             User: %s\n", cur_user);
    fprintf(f, "================================================================================\n");
    
    // Column Headers (Fixed-width for TXT)
    fprintf(f, "  ID | DATE       | TYPE     | CATEGORY           | AMOUNT (Rs) | NOTE\n");
    fprintf(f, "--------------------------------------------------------------------------------\n");
    
    int found_count = 0;
    double total_income = 0.0;
    double total_expense = 0.0;

    for (int i=0;i<txn_count;i++) if (txns[i].month==m && txns[i].year==y) {
        found_count++;
        
        // Accumulate totals
        if (strcmp(txns[i].type, "Income") == 0) total_income += txns[i].amount;
        else total_expense += txns[i].amount;
        
        // Use a temporary variable for note and limit its length for formatting
        char safe_note[192]; snprintf(safe_note, sizeof(safe_note), "%.30s", txns[i].note);
        
        // Print transaction line (ID, Date, Type, Category, Amount, Note)
        fprintf(f, "%4d | %02d/%02d/%04d | %-8s | %-18s | %11.2f | %s\n", 
                txns[i].id, txns[i].day, txns[i].month, txns[i].year, 
                txns[i].type, txns[i].category, txns[i].amount, 
                safe_note);
    }
    
    // Summary Footer
    fprintf(f, "--------------------------------------------------------------------------------\n");
    if (found_count == 0) {
        fprintf(f, "                             No transactions recorded for this period.\n");
    } else {
        fprintf(f, "TOTAL INCOME:                                                 %11.2f\n", total_income);
        fprintf(f, "TOTAL EXPENSE:                                                %11.2f\n", total_expense);
        fprintf(f, "NET BALANCE:                                                  %11.2f\n", total_income - total_expense);
    }
    fprintf(f, "================================================================================\n");

    fclose(f);
    char mmsg[128]; snprintf(mmsg,sizeof(mmsg),"Saved to: %s", fname);
    print_success("Report exported successfully (TXT format).");
    print_centered_in_container(mmsg, C_RESET);
    wait_enter_center();
    print_footer();
}

void settings_menu(void) {
    while (1) {
        print_header("SETTINGS");
        print_left_in_container("1) Change password", C_RESET);
        print_left_in_container("2) About", C_RESET);
        print_left_in_container("0) Back", C_RESET);
        char c[64]; get_input("Choice", c, sizeof(c));
        if (c[0]=='0') { print_footer(); return; }
        if (c[0]=='1') {
            char curp[128], np[128]; 
            get_input("Enter current password", curp, sizeof(curp));
            if (!verify_user_file(cur_user, curp)) { print_error("Incorrect current password."); wait_enter_center(); continue; }
            get_input("Enter new password", np, sizeof(np));
            
            FILE *f = fopen(USERS_CSV, "r"); if (!f) return;
            FILE *t = fopen("users_tmp.csv", "w"); if (!t) { fclose(f); return; }
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), f)) {
                char u[64], penc[128];
                if (sscanf(line, "%63[^,],%127[^\n]", u, penc) == 2) {
                    if (strcmp(u, cur_user) == 0) {
                        char enc[128]; strncpy(enc, np, sizeof(enc)-1); enc[sizeof(enc)-1]='\0'; xor_str(enc);
                        fprintf(t, "%s,%s\n", u, enc);
                    } else fprintf(t, "%s", line);
                }
            }
            fclose(f); fclose(t);
            remove(USERS_CSV); rename("users_tmp.csv", USERS_CSV);
            print_success("Password changed."); wait_enter_center();
        } else if (c[0]=='2') {
            print_header("ABOUT");
            print_centered_in_container("Personal Budget Tracker", C_RESET);
            print_centered_in_container("Developers: Mahandar Kumar & Tushar Kumar", C_RESET);
            print_centered_in_container("FAST-NUCES Karachi", C_RESET);
            print_centered_in_container("This console application was built as a semester project.", C_RESET);
            wait_enter_center();
        } else { print_error("Invalid choice."); wait_enter_center(); }
        print_footer();
    }
}
