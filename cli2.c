#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

// ==========================================
// 1. ENUMS & CONSTANTS & MACROS
// ==========================================
#define MAX_STR 100
#define ID_LEN 20
#define CLINIC_NAME "CITY MULTI-SPECIALTY CLINIC"
#define CLINIC_ADDRESS "123 Healthcare Ave, Enterprise District"
#define CONSULTATION_FEE 500.00

typedef enum {
    REQ_DOC = 0,
    REQ_LAB = 1,
    REQ_BILL = 2,
    COMPLETED = 3,
    CANCELLED_BY_PATIENT = 4,
    ABORTED_CLINICAL = 5
} VisitStatus;

// ==========================================
// 2. ENTERPRISE DATA STRUCTURES (Relational)
// ==========================================
typedef struct {
    int is_active;
    char id[ID_LEN];
    char name[MAX_STR];
    char email[MAX_STR];
    char password[MAX_STR]; // Stored as Hex-XOR encrypted
} Admin;

typedef struct {
    int is_active;
    char id[ID_LEN];
    char name[MAX_STR];
    char email[MAX_STR];
    char password[MAX_STR];
    char shift[50];
    char designation[50]; // NEW: Employee Designation
} Employee;

typedef struct {
    int is_active;
    char id[ID_LEN];
    char name[MAX_STR];
    char email[MAX_STR];
    char password[MAX_STR];
    char qualification[MAX_STR];
    char specialization[MAX_STR]; // NEW: Specialization
    char room_number[10];         // NEW: Room Assignment
    char contact[20];
} Doctor;

typedef struct {
    int is_active;
    char id[ID_LEN];
    char name[MAX_STR];
    char email[MAX_STR];
    char password[MAX_STR];
    char qualification[MAX_STR];
    char contact[20];
} LabAssistant;

// Static Patient Data
typedef struct {
    int is_active;
    char id[ID_LEN];
    char name[MAX_STR];
    char email[MAX_STR];
    char password[MAX_STR];
    int age;
    char gender[15];
    char blood_group[10];
    char contact[20];
    char allergies[MAX_STR]; // CRITICAL FIELD
} Patient;

// Dynamic History Data (The State Machine)
typedef struct {
    char visit_id[ID_LEN];
    char patient_id[ID_LEN];
    char doc_id[ID_LEN];
    char lab_id[ID_LEN];
    VisitStatus status;
    char date[30];
    char ward_bed[20]; // NEW: Ward/Bed Assignment
    
    // Clinical Data
    char vitals[MAX_STR];
    char symptoms[200];
    char diagnosis[200];
    char medicines[200];
    char tests_required[200];
    char test_results[200];
    char doc_advice[200];
    
    // Financial Data
    double doc_fee;
    double med_cost;
    double test_cost;
    double total_bill;
    int is_paid; 
} VisitRecord;

// The Master RAM Database
typedef struct {
    Admin *admins;              int admin_count;    int admin_cap;
    Employee *employees;        int emp_count;      int emp_cap;
    Doctor *doctors;            int doc_count;      int doc_cap;
    LabAssistant *lab_assts;    int lab_count;      int lab_cap;
    Patient *patients;          int pat_count;      int pat_cap;
    VisitRecord *visits;        int visit_count;    int visit_cap;
} Database;

// ==========================================
// FORWARD DECLARATIONS (Fixes Implicit Declaration Warnings)
// ==========================================
void addAdminToDB(Database *db, Admin a);
void addEmployeeToDB(Database *db, Employee e);
void addDoctorToDB(Database *db, Doctor d);
void addLabAsstToDB(Database *db, LabAssistant l);

// ==========================================
// 3. SECURITY & UTILITY ENGINE
// ==========================================
void clearScreen() {
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
}
void pauseSystem() {
    printf("\nPress Enter to continue...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF); 
}
void printHeader(const char* title) {
    clearScreen();
    printf("========================================================\n");
    printf("             %s                 \n", CLINIC_NAME);
    printf("========================================================\n");
    printf(" >> %s\n", title);
    printf("--------------------------------------------------------\n\n");
}

// ==========================================
// FILE LOCK SEMAPHORES (Concurrency Protection)
// ==========================================
void acquireLock() {
    FILE *lock;
    // Busy-wait if another process is currently saving
    while ((lock = fopen("db_lock.tmp", "r")) != NULL) {
        fclose(lock);
    }
    // Create the lock
    lock = fopen("db_lock.tmp", "w");
    if (lock) {
        fprintf(lock, "LOCKED");
        fclose(lock);
    }
}

void releaseLock() {
    remove("db_lock.tmp"); // Delete the lock file
}

// SAFE HEX-XOR CIPHER: Prevents Null-Byte & Delimiter File Corruption
void encryptPasswordHex(const char* input, char* output) {
    char key = 'K'; 
    int i;
    for (i = 0; input[i] != '\0'; i++) {
        sprintf(&output[i * 2], "%02X", input[i] ^ key);
    }
    output[i * 2] = '\0';
}

// Replaces strtok. Properly handles empty fields like "||"
char* parseDelimitedString(char** stringp, const char* delim) {
    if (!stringp || !*stringp) return NULL;
    char* start = *stringp;
    char* p = strpbrk(start, delim);
    if (p) {
        *p = '\0';
        *stringp = p + 1;
    } else {
        *stringp = NULL;
    }
    size_t len = strlen(start);
    if (len > 0 && start[len - 1] == '\n') start[len - 1] = '\0';
    return start;
}

// Case-Insensitive Substring Search
char* custom_strcasestr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; ++haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char *h = haystack, *n = needle;
            while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { ++h; ++n; }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

// ==========================================
// 4. BULLETPROOF INPUT ENGINE
// ==========================================
// Replaces scanf("%s"). Prevents Buffer Overflow and reads spaces.
void safeInput(char* buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0'; // Strip newline
        } else {
            // Buffer overflow occurred, flush stdin
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
    }
}

// Bulletproof Integer Input (Stops infinite loops if user types letters)
int getValidInt(int min, int max) {
    int value;
    char buffer[50];
    while (1) {
        safeInput(buffer, sizeof(buffer));
        if (sscanf(buffer, "%d", &value) == 1) {
            if (value >= min && value <= max) return value;
            printf("Value must be between %d and %d. Try again: ", min, max);
        } else {
            printf("Invalid input. Please enter a number: ");
        }
    }
}

// ==========================================
// 5. DYNAMIC ID GENERATOR
// ==========================================
// Parses flat-files to automatically generate IDs like DOC/001 -> DOC/002
void generateUniqueID(const char* filename, const char* prefix, char* newID) {
    FILE* file = fopen(filename, "r");
    int lastNum = 0;
    
    if (file) {
        char line[1024];
        while (fgets(line, sizeof(line), file)) {
            char tempLine[1024];
            strcpy(tempLine, line);
            
            // Format is: isActive|ID|...
            char* token = strtok(tempLine, "|"); 
            if (token != NULL) {
                token = strtok(NULL, "|"); // Grab the ID token
                if (token != NULL) {
                    int num;
                    // Extract integer after the slash (e.g., "DOC/045" -> 45)
                    if (sscanf(token, "%*[^/]/%d", &num) == 1) {
                        if (num > lastNum) lastNum = num;
                    }
                }
            }
        }
        fclose(file);
    }
    // Base Case handled automatically (lastNum = 0, creates 001)
    sprintf(newID, "%s/%03d", prefix, lastNum + 1);
}

// ==========================================
// 6. MASTER MEMORY TEARDOWN (Valgrind Safe)
// ==========================================
void freeDatabase(Database *db) {
    if (db->admins) free(db->admins);
    if (db->employees) free(db->employees);
    if (db->doctors) free(db->doctors);
    if (db->lab_assts) free(db->lab_assts);
    if (db->patients) free(db->patients);
    if (db->visits) free(db->visits);

    db->admins = NULL; db->employees = NULL; db->doctors = NULL;
    db->lab_assts = NULL; db->patients = NULL; db->visits = NULL;

    db->admin_count = 0; db->emp_count = 0; db->doc_count = 0;
    db->lab_count = 0; db->pat_count = 0; db->visit_count = 0;
}

// ==========================================
// 7. SAFE DYNAMIC ALLOCATION WRAPPERS
// ==========================================
void addPatientToDB(Database *db, Patient p) {
    if (db->pat_count >= db->pat_cap) {
        int new_cap = (db->pat_cap == 0) ? 10 : db->pat_cap * 2;
        Patient *temp = (Patient *)realloc(db->patients, new_cap * sizeof(Patient));
        if (!temp) {
            printf("\n[FATAL ERROR] Out of memory. Cannot add Patient.\n");
            return; // Old db->patients is preserved!
        }
        db->patients = temp;
        db->pat_cap = new_cap;
    }
    db->patients[db->pat_count++] = p;
}

void addVisitToDB(Database *db, VisitRecord v) {
    if (db->visit_count >= db->visit_cap) {
        int new_cap = (db->visit_cap == 0) ? 10 : db->visit_cap * 2;
        VisitRecord *temp = (VisitRecord *)realloc(db->visits, new_cap * sizeof(VisitRecord));
        if (!temp) {
            printf("\n[FATAL ERROR] Out of memory. Cannot add Visit.\n");
            return;
        }
        db->visits = temp;
        db->visit_cap = new_cap;
    }
    db->visits[db->visit_count++] = v;
}

// ==========================================
// 8. DATABASE LOAD ENGINE (File -> RAM)
// ==========================================
void loadDatabase(Database *db) {
    char line[1024]; char *token, *rest;

    FILE *fAdm = fopen("admin_db.txt", "r");
    if (fAdm) {
        while (fgets(line, sizeof(line), fAdm)) {
            Admin a = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; a.is_active = atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(a.id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(a.name, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(a.email, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(a.password, token);
            addAdminToDB(db, a);
        } fclose(fAdm);
    }

    FILE *fEmp = fopen("emp_db.txt", "r");
    if (fEmp) {
        while (fgets(line, sizeof(line), fEmp)) {
            Employee e = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; e.is_active = atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(e.id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(e.name, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(e.email, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(e.password, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(e.shift, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(e.designation, token);
            addEmployeeToDB(db, e);
        } fclose(fEmp);
    }

    FILE *fDoc = fopen("doc_db.txt", "r");
    if (fDoc) {
        while (fgets(line, sizeof(line), fDoc)) {
            Doctor d = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; d.is_active = atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.name, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.email, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.password, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.qualification, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.specialization, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.room_number, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(d.contact, token);
            addDoctorToDB(db, d);
        } fclose(fDoc);
    }

    FILE *fLab = fopen("lab_db.txt", "r");
    if (fLab) {
        while (fgets(line, sizeof(line), fLab)) {
            LabAssistant l = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; l.is_active = atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.name, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.email, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.password, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.qualification, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.contact, token);
            addLabAsstToDB(db, l);
        } fclose(fLab);
    }

    FILE *fPat = fopen("patient_db.txt", "r");
    if (fPat) {
        while (fgets(line, sizeof(line), fPat)) {
            Patient p = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; p.is_active = atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.name, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.email, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.password, token);
            token = parseDelimitedString(&rest, "|"); if (token) p.age = atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.gender, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.blood_group, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.contact, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(p.allergies, token);
            addPatientToDB(db, p);
        } fclose(fPat);
    }

    FILE *fVis = fopen("visits_db.txt", "r");
    if (fVis) {
        while (fgets(line, sizeof(line), fVis)) {
            VisitRecord v = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; strcpy(v.visit_id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.patient_id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.doc_id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.lab_id, token);
            token = parseDelimitedString(&rest, "|"); if (token) v.status = (VisitStatus)atoi(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.date, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.ward_bed, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.vitals, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.symptoms, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.diagnosis, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.medicines, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.tests_required, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.test_results, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(v.doc_advice, token);
            token = parseDelimitedString(&rest, "|"); if (token) v.doc_fee = atof(token);
            token = parseDelimitedString(&rest, "|"); if (token) v.med_cost = atof(token);
            token = parseDelimitedString(&rest, "|"); if (token) v.test_cost = atof(token);
            token = parseDelimitedString(&rest, "|"); if (token) v.total_bill = atof(token);
            token = parseDelimitedString(&rest, "|"); if (token) v.is_paid = atoi(token);
            addVisitToDB(db, v);
        } fclose(fVis);
    }
}

// ==========================================
// 9. DATABASE SAVE ENGINE (RAM -> File)
// ==========================================
void saveDatabase(Database *db) {
    acquireLock();

    FILE *fAdm = fopen("admin_db.txt", "w");
    if (fAdm) {
        for (int i = 0; i < db->admin_count; i++) 
            fprintf(fAdm, "%d|%s|%s|%s|%s\n", db->admins[i].is_active, db->admins[i].id, db->admins[i].name, db->admins[i].email, db->admins[i].password);
        fclose(fAdm);
    }

    FILE *fEmp = fopen("emp_db.txt", "w");
    if (fEmp) {
        for (int i = 0; i < db->emp_count; i++) 
            fprintf(fEmp, "%d|%s|%s|%s|%s|%s|%s\n", db->employees[i].is_active, db->employees[i].id, db->employees[i].name, db->employees[i].email, db->employees[i].password, db->employees[i].shift, db->employees[i].designation);
        fclose(fEmp);
    }

    FILE *fDoc = fopen("doc_db.txt", "w");
    if (fDoc) {
        for (int i = 0; i < db->doc_count; i++) 
            fprintf(fDoc, "%d|%s|%s|%s|%s|%s|%s|%s|%s\n", db->doctors[i].is_active, db->doctors[i].id, db->doctors[i].name, db->doctors[i].email, db->doctors[i].password, db->doctors[i].qualification, db->doctors[i].specialization, db->doctors[i].room_number, db->doctors[i].contact);
        fclose(fDoc);
    }

    FILE *fLab = fopen("lab_db.txt", "w");
    if (fLab) {
        for (int i = 0; i < db->lab_count; i++) 
            fprintf(fLab, "%d|%s|%s|%s|%s|%s|%s\n", db->lab_assts[i].is_active, db->lab_assts[i].id, db->lab_assts[i].name, db->lab_assts[i].email, db->lab_assts[i].password, db->lab_assts[i].qualification, db->lab_assts[i].contact);
        fclose(fLab);
    }

    FILE *fPat = fopen("patient_db.txt", "w");
    if (fPat) {
        for (int i = 0; i < db->pat_count; i++) 
            fprintf(fPat, "%d|%s|%s|%s|%s|%d|%s|%s|%s|%s\n", db->patients[i].is_active, db->patients[i].id, db->patients[i].name, db->patients[i].email, db->patients[i].password, db->patients[i].age, db->patients[i].gender, db->patients[i].blood_group, db->patients[i].contact, db->patients[i].allergies);
        fclose(fPat);
    }

    FILE *fVis = fopen("visits_db.txt", "w");
    if (fVis) {
        for (int i = 0; i < db->visit_count; i++) {
            VisitRecord v = db->visits[i];
            fprintf(fVis, "%s|%s|%s|%s|%d|%s|%s|%s|%s|%s|%s|%s|%s|%s|%.2f|%.2f|%.2f|%.2f|%d\n",
                    v.visit_id, v.patient_id, v.doc_id, v.lab_id, v.status, v.date, v.ward_bed,
                    v.vitals, v.symptoms, v.diagnosis, v.medicines, v.tests_required,
                    v.test_results, v.doc_advice, v.doc_fee, v.med_cost, v.test_cost,
                    v.total_bill, v.is_paid);
        }
        fclose(fVis);
    }
    releaseLock();
}

// ==========================================
// 10. ROLE MEMORY WRAPPERS (Completing Phase 2)
// ==========================================
void addAdminToDB(Database *db, Admin a) {
    if (db->admin_count >= db->admin_cap) {
        int new_cap = (db->admin_cap == 0) ? 10 : db->admin_cap * 2;
        Admin *temp = (Admin *)realloc(db->admins, new_cap * sizeof(Admin));
        if (!temp) { printf("\n[FATAL ERROR] Out of memory.\n"); return; }
        db->admins = temp; db->admin_cap = new_cap;
    }
    db->admins[db->admin_count++] = a;
}

void addEmployeeToDB(Database *db, Employee e) {
    if (db->emp_count >= db->emp_cap) {
        int new_cap = (db->emp_cap == 0) ? 10 : db->emp_cap * 2;
        Employee *temp = (Employee *)realloc(db->employees, new_cap * sizeof(Employee));
        if (!temp) { printf("\n[FATAL ERROR] Out of memory.\n"); return; }
        db->employees = temp; db->emp_cap = new_cap;
    }
    db->employees[db->emp_count++] = e;
}

void addDoctorToDB(Database *db, Doctor d) {
    if (db->doc_count >= db->doc_cap) {
        int new_cap = (db->doc_cap == 0) ? 10 : db->doc_cap * 2;
        Doctor *temp = (Doctor *)realloc(db->doctors, new_cap * sizeof(Doctor));
        if (!temp) { printf("\n[FATAL ERROR] Out of memory.\n"); return; }
        db->doctors = temp; db->doc_cap = new_cap;
    }
    db->doctors[db->doc_count++] = d;
}

void addLabAsstToDB(Database *db, LabAssistant l) {
    if (db->lab_count >= db->lab_cap) {
        int new_cap = (db->lab_cap == 0) ? 10 : db->lab_cap * 2;
        LabAssistant *temp = (LabAssistant *)realloc(db->lab_assts, new_cap * sizeof(LabAssistant));
        if (!temp) { printf("\n[FATAL ERROR] Out of memory.\n"); return; }
        db->lab_assts = temp; db->lab_cap = new_cap;
    }
    db->lab_assts[db->lab_count++] = l;
}

// ==========================================
// 11. SECURITY & VALIDATION ENGINE
// ==========================================
// Ensures no two ACTIVE users share an email across the entire clinic
bool isEmailUnique(Database *db, const char *email) {
    for (int i = 0; i < db->admin_count; i++) 
        if (db->admins[i].is_active && strcmp(db->admins[i].email, email) == 0) return false;
    for (int i = 0; i < db->emp_count; i++) 
        if (db->employees[i].is_active && strcmp(db->employees[i].email, email) == 0) return false;
    for (int i = 0; i < db->doc_count; i++) 
        if (db->doctors[i].is_active && strcmp(db->doctors[i].email, email) == 0) return false;
    for (int i = 0; i < db->lab_count; i++) 
        if (db->lab_assts[i].is_active && strcmp(db->lab_assts[i].email, email) == 0) return false;
    for (int i = 0; i < db->pat_count; i++) 
        if (db->patients[i].is_active && strcmp(db->patients[i].email, email) == 0) return false;
    return true;
}

// Generates Timestamp for the Audit Log
void getCurrentTimestamp(char *buffer) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(buffer, "%02d-%02d-%04d %02d:%02d:%02d", 
            tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, 
            tm.tm_hour, tm.tm_min, tm.tm_sec);
}


// Writes to the hidden tracking file with Sequence ID
void writeAuditLog(const char *actor_id, const char *action, const char *target_id) {
    static int log_sequence = 1; // Auto-increments during runtime
    FILE *f = fopen("audit_log.txt", "a");
    if (f) {
        char timeStr[50];
        getCurrentTimestamp(timeStr);
        fprintf(f, "LOG_%04d | [%s] | %s | %s | %s\n", log_sequence++, timeStr, actor_id, action, target_id);
        fclose(f);
    }
}


// ==========================================
// 12. REGISTRATION ENGINE (The Creators)
// ==========================================
void registerAdmin(Database *db) {
    Admin a = {0}; a.is_active = 1;
    generateUniqueID("admin_db.txt", "ADM", a.id);
    
    printf("\n--- REGISTER MASTER ADMIN ---\n");
    printf("Admin ID Auto-Assigned: %s\n", a.id);
    printf("Enter Full Name: "); safeInput(a.name, MAX_STR);
    printf("Enter Email Address: "); safeInput(a.email, MAX_STR);
    
    if (!isEmailUnique(db, a.email)) {
        printf("\n[ERROR] Email already exists in the system. Registration aborted.\n");
        pauseSystem(); return;
    }
    
    char rawPass[45];
    printf("Enter Password: "); safeInput(rawPass, sizeof(rawPass));
    encryptPasswordHex(rawPass, a.password); // NEW: Hex-XOR Encrypt
    
    addAdminToDB(db, a); saveDatabase(db);
    writeAuditLog(a.id, "REGISTERED_NEW_ADMIN", a.id);
    printf("\n[SUCCESS] Master Admin Registered.\n"); pauseSystem();
}

void registerEmployee(Database *db, const char* admin_id) {
    Employee e = {0}; e.is_active = 1;
    generateUniqueID("emp_db.txt", "EMP", e.id);
    
    printf("\n--- REGISTER NEW EMPLOYEE ---\n");
    printf("Employee ID Auto-Assigned: %s\n", e.id);
    printf("Enter Full Name: "); safeInput(e.name, MAX_STR);
    printf("Enter Email Address: "); safeInput(e.email, MAX_STR);
    
    if (!isEmailUnique(db, e.email)) {
        printf("\n[ERROR] Email already exists. Registration aborted.\n");
        pauseSystem(); return;
    }
    
    char rawPass[45];
    printf("Enter Temporary Password: "); safeInput(rawPass, sizeof(rawPass));
    encryptPasswordHex(rawPass, e.password); // NEW: Hex-XOR Encrypt
    
    printf("Enter Assigned Shift (e.g., Morning/Night): "); safeInput(e.shift, 50);
    printf("Enter Official Designation (e.g., Sr. Receptionist): "); safeInput(e.designation, 50); // NEW: Designation
    
    addEmployeeToDB(db, e); saveDatabase(db);
    writeAuditLog(admin_id, "REGISTERED_EMPLOYEE", e.id);
    printf("\n[SUCCESS] Employee Registered Successfully.\n"); pauseSystem();
}

void registerDoctor(Database *db, const char* admin_id) {
    Doctor d = {0}; d.is_active = 1;
    generateUniqueID("doc_db.txt", "DOC", d.id);
    
    printf("\n--- REGISTER NEW DOCTOR ---\n");
    printf("Doctor ID Auto-Assigned: %s\n", d.id);
    printf("Enter Full Name (with Dr.): "); safeInput(d.name, MAX_STR);
    printf("Enter Email Address: "); safeInput(d.email, MAX_STR);
    
    if (!isEmailUnique(db, d.email)) {
        printf("\n[ERROR] Email already exists. Registration aborted.\n");
        pauseSystem(); return;
    }
    
    char rawPass[45];
    printf("Enter Temporary Password: "); safeInput(rawPass, sizeof(rawPass));
    encryptPasswordHex(rawPass, d.password); 
    
    printf("Enter Qualifications (e.g., MBBS, MD): "); safeInput(d.qualification, MAX_STR);
    
    // YOUR CUSTOM FEATURES IMPLEMENTED HERE:
    printf("Enter Specialization (e.g., Orthopedics, General): "); safeInput(d.specialization, MAX_STR);
    printf("Enter Assigned Room Number: "); safeInput(d.room_number, 10);
    printf("Enter Doctor's Contact Phone: "); safeInput(d.contact, 20);
    
    addDoctorToDB(db, d); saveDatabase(db);
    writeAuditLog(admin_id, "REGISTERED_DOCTOR", d.id);
    printf("\n[SUCCESS] Doctor Registered Successfully.\n"); pauseSystem();
}
void registerLabAssistant(Database *db, const char* admin_id) {
    LabAssistant l = {0}; l.is_active = 1;
    generateUniqueID("lab_db.txt", "LAB", l.id);
    printf("\n--- REGISTER NEW LAB ASSISTANT ---\n");
    printf("Lab Assistant ID Auto-Assigned: %s\n", l.id);
    printf("Enter Full Name: "); safeInput(l.name, MAX_STR);
    printf("Enter Email Address: "); safeInput(l.email, MAX_STR);
    if (!isEmailUnique(db, l.email)) { printf("\n[ERROR] Email exists.\n"); pauseSystem(); return; }
    
    char rawPass[45]; printf("Enter Temporary Password: "); safeInput(rawPass, sizeof(rawPass));
    encryptPasswordHex(rawPass, l.password); 
    
    printf("Enter Qualifications (e.g., BSc, DMLT): "); safeInput(l.qualification, MAX_STR);
    printf("Enter Contact Phone Number: "); safeInput(l.contact, 20);
    
    addLabAsstToDB(db, l); saveDatabase(db);
    writeAuditLog(admin_id, "REGISTERED_LAB_ASST", l.id);
    printf("\n[SUCCESS] Lab Assistant Registered.\n"); pauseSystem();
}

void registerPatient(Database *db, const char* emp_id) {
    Patient p = {0}; p.is_active = 1;
    generateUniqueID("patient_db.txt", "P26", p.id);
    printf("\n--- PATIENT INTAKE & REGISTRATION ---\n");
    printf("Auto-Assigned ID: %s\n", p.id);
    printf("Name: "); safeInput(p.name, MAX_STR);
    printf("Email: "); safeInput(p.email, MAX_STR);
    if(!isEmailUnique(db, p.email)) { printf("[ERROR] Email in use.\n"); pauseSystem(); return; }
    
    char raw[45]; printf("Temporary Portal Password: "); safeInput(raw, sizeof(raw)); 
    encryptPasswordHex(raw, p.password);
    
    printf("Age: "); p.age = getValidInt(0, 120);
    printf("Gender: "); safeInput(p.gender, 15);
    printf("Blood Group: "); safeInput(p.blood_group, 10);
    printf("Contact Phone Number: "); safeInput(p.contact, 20); 
    printf("Known Allergies (CRITICAL - Type 'None' if none): "); safeInput(p.allergies, MAX_STR);
    
   addPatientToDB(db, p); saveDatabase(db);
    writeAuditLog(emp_id, "REGISTERED_PATIENT", p.id);
    printf("\n[SUCCESS] Patient Registration Complete.\n"); pauseSystem();
}

void forgotPasswordRecovery(Database *db) {
    char email[MAX_STR], phone[20];
    printHeader("FORGOT PASSWORD RECOVERY");
    printf("Enter your registered Email: "); safeInput(email, MAX_STR);
    printf("Enter your registered Phone Number: "); safeInput(phone, 20);

    for (int i = 0; i < db->pat_count; i++) {
        // Verifies both Email AND Phone Number match
        if (db->patients[i].is_active && strcmp(db->patients[i].email, email) == 0 && strcmp(db->patients[i].contact, phone) == 0) {
            printf("\n[IDENTITY VERIFIED] Account found: %s\n", db->patients[i].name);
            char rawPass[45];
            printf("Enter NEW Password: "); safeInput(rawPass, sizeof(rawPass));
            
            encryptPasswordHex(rawPass, db->patients[i].password);
            saveDatabase(db);
            writeAuditLog(db->patients[i].id, "PASSWORD_RESET", "KIOSK");
            
            printf("[SUCCESS] Password updated. You may now log in.\n");
            pauseSystem();
            return;
        }
    }
    printf("\n[ERROR] No matching active account found with that Email and Phone combination.\n");
    pauseSystem();
}

// ==========================================
// AUTHENTICATION ENGINES
// ==========================================
Admin* loginAdmin(Database *db) {
    char email[MAX_STR], pass[MAX_STR], encPass[MAX_STR];
    printHeader("MASTER ADMIN SECURE LOGIN");
    printf("Enter Email: "); safeInput(email, MAX_STR);
    printf("Enter Password: "); safeInput(pass, MAX_STR);
    
    encryptPasswordHex(pass, encPass); // NEW: Hex-XOR
    
    for (int i = 0; i < db->admin_count; i++) {
        if (db->admins[i].is_active && strcmp(db->admins[i].email, email) == 0 && strcmp(db->admins[i].password, encPass) == 0) {
            writeAuditLog(db->admins[i].id, "LOGGED_IN", "SYSTEM");
            return &db->admins[i];
        }
    }
    printf("\n[ERROR] Invalid Credentials or Account Disabled.\n"); pauseSystem(); return NULL;
}

Doctor* loginDoctor(Database *db) {
    char email[MAX_STR], pass[MAX_STR], encPass[MAX_STR];
    printHeader("DOCTOR CLINICAL PORTAL");
    printf("Enter Email: "); safeInput(email, MAX_STR);
    printf("Enter Password: "); safeInput(pass, MAX_STR);
    
    encryptPasswordHex(pass, encPass); // NEW: Hex-XOR
    
    for (int i = 0; i < db->doc_count; i++) {
        if (db->doctors[i].is_active && strcmp(db->doctors[i].email, email) == 0 && strcmp(db->doctors[i].password, encPass) == 0) {
            writeAuditLog(db->doctors[i].id, "LOGGED_IN", "SYSTEM");
            return &db->doctors[i];
        }
    }
    printf("\n[ERROR] Invalid Credentials or Account Disabled.\n"); pauseSystem(); return NULL;
}

LabAssistant* loginLabAssistant(Database *db) {
    char email[MAX_STR], pass[MAX_STR], encPass[MAX_STR];
    printHeader("LAB TECHNICIAN PORTAL");
    printf("Enter Email: "); safeInput(email, MAX_STR);
    printf("Enter Password: "); safeInput(pass, MAX_STR);
    
    encryptPasswordHex(pass, encPass); // NEW: Hex-XOR
    
    for (int i = 0; i < db->lab_count; i++) {
        if (db->lab_assts[i].is_active && strcmp(db->lab_assts[i].email, email) == 0 && strcmp(db->lab_assts[i].password, encPass) == 0) {
            writeAuditLog(db->lab_assts[i].id, "LOGGED_IN", "SYSTEM");
            return &db->lab_assts[i];
        }
    }
    printf("\n[ERROR] Invalid Credentials.\n"); pauseSystem(); return NULL;
}

Patient* loginPatient(Database *db) {
    char email[MAX_STR], pass[MAX_STR], encPass[MAX_STR];
    printHeader("PATIENT SELF-SERVICE KIOSK");
    printf("Enter Email: "); safeInput(email, MAX_STR);
    printf("Enter Password: "); safeInput(pass, MAX_STR);
    
    encryptPasswordHex(pass, encPass); // NEW: Hex-XOR
    
    for (int i = 0; i < db->pat_count; i++) {
        if (db->patients[i].is_active && strcmp(db->patients[i].email, email) == 0 && strcmp(db->patients[i].password, encPass) == 0) {
            writeAuditLog(db->patients[i].id, "LOGGED_IN", "KIOSK");
            return &db->patients[i];
        }
    }
    printf("\n[ERROR] Invalid Credentials.\n"); pauseSystem(); return NULL;
}

// ==========================================
// 14. RELATIONAL HELPERS (Foreign Key Lookups)
// ==========================================
Patient* getPatientByID(Database *db, const char* id) {
    for (int i = 0; i < db->pat_count; i++) 
        if (strcmp(db->patients[i].id, id) == 0 && db->patients[i].is_active) return &db->patients[i];
    return NULL;
}

Doctor* getDoctorByID(Database *db, const char* id) {
    for (int i = 0; i < db->doc_count; i++) 
        if (strcmp(db->doctors[i].id, id) == 0 && db->doctors[i].is_active) return &db->doctors[i];
    return NULL;
}

// Format Name for Filenames (John Doe -> John_Doe)
void formatFilename(char* dest, const char* name, const char* suffix) {
    strcpy(dest, name);
    for (int i = 0; dest[i]; i++) {
        if (dest[i] == ' ') dest[i] = '_';
    }
    strcat(dest, suffix);
}

// ==========================================
// 15. THE EXPORT ENGINE (Document Generation)
// ==========================================
void exportPrescription(Database *db, VisitRecord *v) {
    Patient *p = getPatientByID(db, v->patient_id);
    Doctor *d = getDoctorByID(db, v->doc_id);
    if (!p || !d) return;

    char filename[MAX_STR];
    formatFilename(filename, p->name, "_Rx.txt");
    FILE *f = fopen(filename, "a"); // FIXED: Append Mode
    if (f) {
        fprintf(f, "\n\n*************************************************\n");
        fprintf(f, "               NEW PRESCRIPTION ENTRY            \n");
        char timeStr[50]; getCurrentTimestamp(timeStr);
        fprintf(f, "=================================================\n");
        fprintf(f, "         %s \n", CLINIC_NAME);
        fprintf(f, "         %s \n", CLINIC_ADDRESS);
        fprintf(f, " Dr. %s | %s (%s)\n", d->name, d->qualification, d->specialization);
        fprintf(f, "=================================================\n");
        fprintf(f, "Date: %s | Visit ID: %s | Room: %s\n", timeStr, v->visit_id, d->room_number);
        fprintf(f, "Patient: %-20s ID: %s\n", p->name, p->id);
        fprintf(f, "Age: %-5d Gender: %-10s Blood: %s\n", p->age, p->gender, p->blood_group);
        fprintf(f, "Allergies: %s\n", p->allergies);
        fprintf(f, "-------------------------------------------------\n");
        
        // Dynamic Medical History Pull
        fprintf(f, "[PAST MEDICAL HISTORY]\n");
        int history_found = 0;
        for (int i = 0; i < db->visit_count; i++) {
            if (strcmp(db->visits[i].patient_id, p->id) == 0 && db->visits[i].status == COMPLETED && strcmp(db->visits[i].visit_id, v->visit_id) != 0) {
                fprintf(f, " - %s: %s\n", db->visits[i].date, db->visits[i].diagnosis);
                history_found = 1;
            }
        }
        if (!history_found) fprintf(f, " - No previous history on record.\n");
        
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "[VITALS] %s\n\n", v->vitals);
        fprintf(f, "SYMPTOMS:  %s\n", v->symptoms);
        fprintf(f, "DIAGNOSIS: %s\n\n", v->diagnosis);
        fprintf(f, "Rx (MEDICINES):\n%s\n\n", v->medicines);
        fprintf(f, "ADVICE:\n%s\n", v->doc_advice);
        if (v->status == REQ_LAB) fprintf(f, "\n*TESTS ADVISED: %s\n", v->tests_required);
        fprintf(f, "=================================================\n");
        fclose(f);
        printf("\n[SYSTEM] Exported Prescription to: %s\n", filename);
    }
}

void exportInvoice(Database *db, VisitRecord *v) {
    Patient *p = getPatientByID(db, v->patient_id);
    if (!p) return;

    char filename[MAX_STR];
    formatFilename(filename, p->name, "_Bill.txt");
    FILE *f = fopen(filename, "a"); // FIXED: Append Mode
    if (f) {
        fprintf(f, "\n\n*************************************************\n");
        fprintf(f, "                 NEW INVOICE ENTRY               \n");
        char timeStr[50]; getCurrentTimestamp(timeStr);
        double subtotal = v->doc_fee + v->med_cost + v->test_cost;
        double tax = subtotal * 0.05;
        v->total_bill = subtotal + tax;

        fprintf(f, "=================================================\n");
        fprintf(f, "         CITY CLINIC - TAX INVOICE               \n");
        fprintf(f, "=================================================\n");
        fprintf(f, "Date: %s | Invoice ID: INV-%s\n", timeStr, v->visit_id);
        fprintf(f, "Patient: %-20s ID: %s\n", p->name, p->id);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "DESCRIPTION                     AMOUNT ($)       \n");
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "Consultation Fee                %10.2f\n", v->doc_fee);
        fprintf(f, "Pharmacy / Medicines            %10.2f\n", v->med_cost);
        fprintf(f, "Laboratory Tests                %10.2f\n", v->test_cost);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "SUBTOTAL                        %10.2f\n", subtotal);
        fprintf(f, "TAX (5%%)                        %10.2f\n", tax);
        fprintf(f, "=================================================\n");
        fprintf(f, "GRAND TOTAL                     %10.2f\n", v->total_bill);
        fprintf(f, "STATUS: %s\n", v->is_paid ? "[ PAID ]" : "[ UNPAID ]");
        fprintf(f, "=================================================\n");
        fclose(f);
        printf("\n[SYSTEM] Exported Invoice to: %s\n", filename);
    }
}

// ==========================================
// 15.5 CRUD & UNIVERSAL SEARCH ENGINES
// ==========================================

Patient* searchPatient(Database *db) {
    printf("\n--- SEARCH PATIENT DIRECTORY ---\n");
    char query[MAX_STR];
    printf("Enter Patient Email or Phone Number: "); safeInput(query, MAX_STR);
    
    for (int i = 0; i < db->pat_count; i++) {
        if (db->patients[i].is_active && (strcmp(db->patients[i].email, query) == 0 || strcmp(db->patients[i].contact, query) == 0)) {
            printf("\n[FOUND] ID: %s | Name: %s | Age: %d | Blood: %s | Allergies: %s\n", 
                   db->patients[i].id, db->patients[i].name, db->patients[i].age, db->patients[i].blood_group, db->patients[i].allergies);
            return &db->patients[i];
        }
    }
    printf("[ERROR] No active patient found with those details.\n");
    return NULL;
}

void displayPatientHistory(Database *db, Patient *p) {
    printf("\n========================================================\n");
    printf("        COMPREHENSIVE MEDICAL HISTORY: %s\n", p->name);
    printf("========================================================\n");
    int found = 0;
    for (int i = 0; i < db->visit_count; i++) {
        if (strcmp(db->visits[i].patient_id, p->id) == 0 && db->visits[i].status >= REQ_BILL) {
            Doctor *d = getDoctorByID(db, db->visits[i].doc_id);
            printf("\n[Date: %s] | Visit ID: %s | Status: %s\n", db->visits[i].date, db->visits[i].visit_id, db->visits[i].is_paid ? "PAID" : "UNPAID");
            printf("Attending Physician: Dr. %s\n", d ? d->name : "Unknown");
            printf("Vitals: %s\n", db->visits[i].vitals);
            printf("Diagnosis: %s\n", db->visits[i].diagnosis);
            printf("Prescription: %s\n", db->visits[i].medicines);
            if (strcmp(db->visits[i].tests_required, "None") != 0) {
                printf("Lab Tests: %s | Results: %s\n", db->visits[i].tests_required, db->visits[i].test_results);
            }
            printf("--------------------------------------------------------\n");
            found = 1;
        }
    }
    if (!found) printf("No prior completed visits found for this patient.\n");
    pauseSystem();
}

void updateOwnPassword(char* encryptedPassStr) {
    char raw[45];
    printf("\n--- UPDATE PROFILE PASSWORD ---\n");
    printf("Enter New Password: "); safeInput(raw, sizeof(raw));
    encryptPasswordHex(raw, encryptedPassStr);
    printf("[SUCCESS] Password updated successfully.\n");
    pauseSystem();
}

void adminManageDirectory(Database *db) {
    printHeader("DIRECTORY MANAGEMENT & PAGINATION");
    printf("View Directory: (1) Doctors, (2) Employees, (3) Lab Assts, (4) Patients\nChoice: ");
    int type = getValidInt(1, 4);
    int count = 0, total = 0;
    printf("\n--- ACTIVE DIRECTORY (Showing 15 per page) ---\n");
    if (type == 1) {
        total = db->doc_count;
        for (int i = 0; i < total; i++) {
            if (db->doctors[i].is_active) {
                printf("[%d] ID: %s | Name: Dr. %s | Spec: %s | Email: %s | Pass(Hex): %s\n", 
                       i, db->doctors[i].id, db->doctors[i].name, db->doctors[i].specialization, db->doctors[i].email, db->doctors[i].password);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    } else if (type == 2) {
        total = db->emp_count;
        for (int i = 0; i < total; i++) {
            if (db->employees[i].is_active) {
                printf("[%d] ID: %s | Name: %s | Desig: %s | Email: %s | Pass(Hex): %s\n", 
                       i, db->employees[i].id, db->employees[i].name, db->employees[i].designation, db->employees[i].email, db->employees[i].password);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    } else if (type == 3) {
        total = db->lab_count;
        for (int i = 0; i < total; i++) {
            if (db->lab_assts[i].is_active) {
                printf("[%d] ID: %s | Name: %s | Qual: %s | Email: %s | Pass(Hex): %s\n", 
                       i, db->lab_assts[i].id, db->lab_assts[i].name, db->lab_assts[i].qualification, db->lab_assts[i].email, db->lab_assts[i].password);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    } else if (type == 4) {
        total = db->pat_count;
        for (int i = 0; i < total; i++) {
            if (db->patients[i].is_active) {
                printf("[%d] ID: %s | Name: %s | Age: %d | Blood: %s | Phone: %s | Allergies: %s\n", 
                       i, db->patients[i].id, db->patients[i].name, db->patients[i].age, db->patients[i].blood_group, db->patients[i].contact, db->patients[i].allergies);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    }
    printf("\nActions: (1) Soft Delete User, (2) Return to Dashboard\nChoice: ");
    if (getValidInt(1, 2) == 1) {
        char target_id[ID_LEN];
        printf("Enter ID to deactivate: "); safeInput(target_id, ID_LEN);
        int success = 0;
        if (type == 1) { for (int i = 0; i < total; i++) if (strcmp(db->doctors[i].id, target_id) == 0) { db->doctors[i].is_active = 0; success = 1; } }
        else if (type == 2) { for (int i = 0; i < total; i++) if (strcmp(db->employees[i].id, target_id) == 0) { db->employees[i].is_active = 0; success = 1; } }
        else if (type == 3) { for (int i = 0; i < total; i++) if (strcmp(db->lab_assts[i].id, target_id) == 0) { db->lab_assts[i].is_active = 0; success = 1; } }
        else if (type == 4) { for (int i = 0; i < total; i++) if (strcmp(db->patients[i].id, target_id) == 0) { db->patients[i].is_active = 0; success = 1; } }
        
        if (success) { saveDatabase(db); printf("[SUCCESS] User Account Deactivated.\n"); }
        else { printf("[ERROR] ID not found.\n"); }
        pauseSystem();
    }
}

void adminUpdateUser(Database *db, const char* admin_id) {
    printHeader("ADMIN: UPDATE USER DETAILS");
    printf("Select Role to Edit: (1) Doctor, (2) Employee, (3) Lab Asst, (4) Patient, (5) Admin\nChoice: ");
    int role = getValidInt(1, 5);

    char target_id[ID_LEN];
    printf("Enter User ID to Update: "); safeInput(target_id, ID_LEN);

    int found = 0;
    char *namePtr = NULL, *emailPtr = NULL, *phonePtr = NULL;

    if (role == 1) {
        for (int i = 0; i < db->doc_count; i++) 
            if (strcmp(db->doctors[i].id, target_id) == 0) { found=1; namePtr=db->doctors[i].name; emailPtr=db->doctors[i].email; phonePtr=db->doctors[i].contact; break; }
    } else if (role == 2) {
        for (int i = 0; i < db->emp_count; i++) 
            if (strcmp(db->employees[i].id, target_id) == 0) { found=1; namePtr=db->employees[i].name; emailPtr=db->employees[i].email; break; }
    } else if (role == 3) {
        for (int i = 0; i < db->lab_count; i++) 
            if (strcmp(db->lab_assts[i].id, target_id) == 0) { found=1; namePtr=db->lab_assts[i].name; emailPtr=db->lab_assts[i].email; phonePtr=db->lab_assts[i].contact; break; }
    } else if (role == 4) {
        for (int i = 0; i < db->pat_count; i++) 
            if (strcmp(db->patients[i].id, target_id) == 0) { found=1; namePtr=db->patients[i].name; emailPtr=db->patients[i].email; phonePtr=db->patients[i].contact; break; }
    } else if (role == 5) {
        for (int i = 0; i < db->admin_count; i++) 
            if (strcmp(db->admins[i].id, target_id) == 0) { found=1; namePtr=db->admins[i].name; emailPtr=db->admins[i].email; break; }
    }

    if (found) {
        printf("\nUser Found: %s\n", namePtr);
        printf("What would you like to update?\n(1) Name\n(2) Email\n(3) Phone Number\nChoice: ");
        int ch = getValidInt(1, 4);
        
        if (ch == 1) { 
            printf("Enter New Name: "); safeInput(namePtr, MAX_STR); 
        }
        else if (ch == 2) { 
            char newEmail[MAX_STR]; printf("Enter New Email: "); safeInput(newEmail, MAX_STR);
            if (isEmailUnique(db, newEmail)) strcpy(emailPtr, newEmail);
            else { printf("[ERROR] Email already in use.\n"); pauseSystem(); return; }
        }
        else if (ch == 3 && phonePtr != NULL) { 
            printf("Enter New Phone: "); safeInput(phonePtr, 20); 
        }
        else if (ch == 3 && phonePtr == NULL) { 
            printf("[ERROR] This role does not store a phone number.\n"); 
        }
        else if (ch == 4) {
            if (role == 1) { // Doctor
                for (int i=0; i<db->doc_count; i++) if (strcmp(db->doctors[i].id, target_id) == 0) {
                    printf("Enter New Specialization (current: %s): ", db->doctors[i].specialization);
                    safeInput(db->doctors[i].specialization, MAX_STR); break;
                }
            } else if (role == 2) { // Employee
                for (int i=0; i<db->emp_count; i++) if (strcmp(db->employees[i].id, target_id) == 0) {
                    printf("Update (1) Shift or (2) Designation? Choice: ");
                    if (getValidInt(1,2) == 1) { printf("New Shift: "); safeInput(db->employees[i].shift, 50); }
                    else { printf("New Designation: "); safeInput(db->employees[i].designation, 50); }
                    break;
                }
            } else if (role == 3) { // Lab Asst
                for (int i=0; i<db->lab_count; i++) if (strcmp(db->lab_assts[i].id, target_id) == 0) {
                    printf("Enter New Qualification (current: %s): ", db->lab_assts[i].qualification);
                    safeInput(db->lab_assts[i].qualification, MAX_STR); break;
                }
            } else if (role == 4) { // Patient
                for (int i=0; i<db->pat_count; i++) if (strcmp(db->patients[i].id, target_id) == 0) {
                    printf("Enter Updated Allergies (current: %s): ", db->patients[i].allergies);
                    safeInput(db->patients[i].allergies, MAX_STR); break;
                }
            }
        }
        
        saveDatabase(db);
        writeAuditLog(admin_id, "UPDATED_USER_PROFILE", target_id);
        printf("\n[SUCCESS] User details updated successfully.\n");
    } else { printf("\n[ERROR] User ID not found.\n"); }
    pauseSystem();
}

// ==========================================
// 16. DASHBOARDS (The Routing Engine)
// ==========================================

void doctorDashboard(Database *db, Doctor *doc) {
    int choice;
    do {
        printHeader("DOCTOR CLINICAL DASHBOARD");
        printf("Welcome, Dr. %s\n\n", doc->name);
        printf("1. View My Patient Queue (REQ_DOC)\n");
        printf("2. Conduct Patient Visit\n");
        printf("3. Update Old Prescription\n");
        printf("4. Search Old Patient & View History\n");
        printf("5. Update My Profile\n");
        printf("6. Logout\nChoice: ");
        choice = getValidInt(1, 6);
        if (choice == 1) {
            printf("\n--- WAITING ROOM ---\n");
            int found = 0;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].doc_id, doc->id) == 0 && db->visits[i].status == REQ_DOC) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if(p) {
                        printf("Visit ID: %s | Patient: %s | Age: %d | Allergies: %s\n", 
                               db->visits[i].visit_id, p->name, p->age, p->allergies);
                        found = 1;
                    }
                }
            }
            if (!found) printf("No patients waiting in your queue.\n");
            pauseSystem();
        } 
        else if (choice == 2) {
            char v_id[ID_LEN];
            printf("\nEnter Visit ID from Queue: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && strcmp(db->visits[i].doc_id, doc->id) == 0) {
                    v = &db->visits[i]; break;
                }
            }
            
            if (v && v->status == REQ_DOC) {
                Patient *p = getPatientByID(db, v->patient_id);
                printf("\n--- CLINICAL EVALUATION ---\n");
                printf("Enter Vitals (e.g., BP 120/80): "); safeInput(v->vitals, MAX_STR);
                printf("Enter Symptoms: "); safeInput(v->symptoms, 200);
                printf("Enter Diagnosis: "); safeInput(v->diagnosis, 200);
                
                // ALLERGY WARNING SYSTEM
                while(1) {
                    printf("Enter Medicines Prescribed: "); safeInput(v->medicines, 200);
                    if (custom_strcasestr(p->allergies, "none") == NULL) {
                        printf("\n[CRITICAL WARNING] Patient Allergies: %s\n", p->allergies);
                        printf("Does this prescription conflict with allergies? (1 = Yes, 0 = No): ");
                        if (getValidInt(0, 1) == 1) {
                            printf("--- Please re-prescribe safer alternatives ---\n");
                            continue; // Loop back to ask for medicines again
                        }
                    }
                    break; // Escape loop if safe
                }
                
                printf("Enter Doctor's Advice: "); safeInput(v->doc_advice, 200);

                // WARD ADMISSION LOGIC
                printf("\nAdmit Patient to Ward/Bed? (1 = Yes, 0 = No): ");
                if(getValidInt(0, 1) == 1) {
                    printf("Specify Ward/Bed instructions: "); safeInput(v->ward_bed, 20);
                } else {
                    strcpy(v->ward_bed, "Outpatient");
                }

                printf("\nDoes this patient need Lab Tests? (1 = Yes, 0 = No): ");
                int needs_lab = getValidInt(0, 1);
                
                if (needs_lab) {
                    printf("Specify Tests (e.g., Blood, X-Ray): "); safeInput(v->tests_required, 200);
                    v->status = REQ_LAB; 
                    printf("\n[ROUTING] Patient sent to Lab Queue.\n");
                } else {
                    strcpy(v->tests_required, "None");
                    v->status = REQ_BILL; 
                    printf("\n[ROUTING] Patient sent to Billing Queue.\n");
                }
                
                exportPrescription(db, v);
                saveDatabase(db);
                writeAuditLog(doc->id, "COMPLETED_EVALUATION", v->visit_id);
            } else {
                printf("[ERROR] Visit ID not found or not assigned to you.\n");
            }
            pauseSystem();
        }

        else if (choice == 3) {
            char v_id[ID_LEN];
            printf("\nEnter Visit ID to Update Prescription: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && strcmp(db->visits[i].doc_id, doc->id) == 0) {
                    v = &db->visits[i]; break;
                }
            }
            
            if (v && v->status >= REQ_DOC) {
                printf("Current Medicines Prescribed: %s\n", v->medicines);
                printf("Enter additional medicines to append (or 'None' to cancel): ");
                char extra[100]; safeInput(extra, 100);
                
                if (custom_strcasestr(extra, "none") == NULL) {
                    // Safe concatenation to prevent buffer overflow
                    int rem = 199 - strlen(v->medicines);
                    if (rem > 5) {
                        strncat(v->medicines, ", ", rem);
                        strncat(v->medicines, extra, rem - 2);
                        saveDatabase(db);
                        exportPrescription(db, v); // Generates updated Rx.txt
                        writeAuditLog(doc->id, "UPDATED_PRESCRIPTION", v->visit_id);
                        printf("[SUCCESS] Prescription updated & document re-exported.\n");
                    } else { 
                        printf("[ERROR] Prescription buffer is full. Cannot add more.\n"); 
                    }
                }
           } else { 
                printf("[ERROR] Visit ID not found or unauthorized.\n"); 
            }
            pauseSystem();
        }
        else if (choice == 4) {
            Patient *found_pat = searchPatient(db);
            if (found_pat) displayPatientHistory(db, found_pat);
        }
        else if (choice == 5) { 
            printHeader("UPDATE MY PROFILE");
            printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Contact Phone\n(4) Specialization\nChoice: ");
            int ch = getValidInt(1, 4);
            
            if (ch == 1) {
                printf("Enter New Name: "); safeInput(doc->name, MAX_STR);
                printf("[SUCCESS] Name updated.\n");
            } else if (ch == 2) {
                char raw[45];
                printf("Enter New Password: "); safeInput(raw, sizeof(raw));
                encryptPasswordHex(raw, doc->password);
                printf("[SUCCESS] Password updated.\n");
            } else if (ch == 3) {
                printf("Enter New Phone: "); safeInput(doc->contact, 20);
                printf("[SUCCESS] Phone updated.\n");
            } else if (ch == 4) {
                printf("Enter New Specialization: "); safeInput(doc->specialization, MAX_STR);
                printf("[SUCCESS] Specialization updated.\n");
            }
            saveDatabase(db);
            writeAuditLog(doc->id, "UPDATED_OWN_PROFILE", doc->id);
            pauseSystem();
        }
    } while (choice != 6);
}

void employeeDashboard(Database *db, Employee *emp) {
    int choice;
    do {
        printHeader("RECEPTION & BILLING DASHBOARD");
        printf("Welcome, %s\n\n", emp->name);
        printf(" 1. Register New Walk-in Patient\n");
        printf(" 2. View Doctor Directory\n");
        printf(" 3. Create Visit (Assign Patient to Doctor)\n");
        printf(" 4. Update Patient Demographics\n");
        printf(" 5. Search Old Patient & View History\n");
        printf(" 6. View All Active Patients (Directory)\n");
        printf(" 7. Process Billing & Checkout (REQ_BILL)\n");
        printf(" 8. Discharge Patient\n");
        printf(" 9. Update My Profile\n");
        printf(" 10. Logout\nChoice: ");
        choice = getValidInt(1, 10);

       if (choice == 1) {
            registerPatient(db, emp->id); 
        }
        else if (choice == 2) {
            printf("\n--- DOCTOR DIRECTORY ---\n");
            if (db->doc_count == 0) printf("  No doctors registered.\n");
            for (int i = 0; i < db->doc_count; i++) {
                if (db->doctors[i].is_active) {
                    printf("  ID: %-10s | Dr. %-20s | Spec: %s | Room: %s\n", 
                        db->doctors[i].id, db->doctors[i].name, db->doctors[i].specialization, db->doctors[i].room_number);
                }
            }
            pauseSystem();
        }
        else if (choice == 3) {
            VisitRecord v = {0};
            generateUniqueID("visits_db.txt", "VIS", v.visit_id);
            getCurrentTimestamp(v.date);
            v.status = REQ_DOC; 
            v.is_paid = 0; v.total_bill = 0; v.doc_fee = 0; v.med_cost = 0; v.test_cost = 0;
            
            printf("\n--- ACTIVE PATIENT LIST ---\n");
            int pCount = 0;
            for(int i=0; i<db->pat_count; i++) {
                if(db->patients[i].is_active) {
                    printf(" - ID: %-10s | Name: %s\n", db->patients[i].id, db->patients[i].name);
                    pCount++;
                }
            }
            if (pCount == 0) { printf(" No active patients. Please register one first.\n"); pauseSystem(); continue; }

            printf("\n--- ASSIGN DOCTOR ---\n");
            printf("Enter Patient ID: "); safeInput(v.patient_id, ID_LEN);
            if(!getPatientByID(db, v.patient_id)) { printf("[ERROR] Patient not found.\n"); pauseSystem(); continue; }
            
            printf("\nAvailable Doctors:\n");
            for(int i=0; i<db->doc_count; i++) 
                if(db->doctors[i].is_active) printf(" - ID: %s | Dr. %s (%s) | Room: %s\n", db->doctors[i].id, db->doctors[i].name, db->doctors[i].specialization, db->doctors[i].room_number);
                
            Doctor *assignedDoc = NULL;
            while (assignedDoc == NULL) {
                printf("\nEnter Doctor ID to assign (or type CANCEL): "); 
                safeInput(v.doc_id, ID_LEN);
                if (strcmp(v.doc_id, "CANCEL") == 0) break;
                
                assignedDoc = getDoctorByID(db, v.doc_id);
                if (!assignedDoc) printf("[ERROR] Invalid Doctor ID. Try again.\n");
            }
            if (!assignedDoc) continue; 

            strcpy(v.lab_id, "PENDING"); 

            addVisitToDB(db, v); saveDatabase(db);
            writeAuditLog(emp->id, "CREATED_VISIT", v.visit_id);
            printf("\n[SUCCESS] Visit created! Please direct patient to Dr. %s in Room %s.\n", assignedDoc->name, assignedDoc->room_number); 
            pauseSystem();
        }
        else if (choice == 4) {
            char pId[ID_LEN];
            printf("\nEnter Patient ID to Update: "); safeInput(pId, ID_LEN);
            Patient *p = getPatientByID(db, pId);
            if (!p) { printf("\n[ERROR] Patient not found or inactive.\n"); pauseSystem(); continue; }
            
            printf("\n--- UPDATING: %s ---\n", p->name);
            char tmp[MAX_STR];
            
            printf("New Name (Enter to keep '%s'): ", p->name);
            safeInput(tmp, sizeof(tmp)); if (strlen(tmp) > 0) strcpy(p->name, tmp);
            
            printf("New Email (Enter to keep '%s'): ", p->email);
            safeInput(tmp, sizeof(tmp)); 
            if (strlen(tmp) > 0) {
                if (isEmailUnique(db, tmp) || strcmp(p->email, tmp) == 0) strcpy(p->email, tmp);
                else { printf("[WARNING] Email in use. Keeping old email.\n"); }
            }
            
            printf("New Password (Enter to keep current hidden): ");
            safeInput(tmp, sizeof(tmp)); 
            if (strlen(tmp) > 0) encryptPasswordHex(tmp, p->password);
            
            printf("New Age (Enter to keep '%d'): ", p->age);
            safeInput(tmp, sizeof(tmp)); if (strlen(tmp) > 0) p->age = atoi(tmp);
            
            printf("New Gender (Enter to keep '%s'): ", p->gender);
            safeInput(tmp, sizeof(tmp)); if (strlen(tmp) > 0) strcpy(p->gender, tmp);
            
            printf("New Blood Group (Enter to keep '%s'): ", p->blood_group);
            safeInput(tmp, sizeof(tmp)); if (strlen(tmp) > 0) strcpy(p->blood_group, tmp);
            
            printf("New Contact Phone (Enter to keep '%s'): ", p->contact);
            safeInput(tmp, sizeof(tmp)); if (strlen(tmp) > 0) strcpy(p->contact, tmp);
            
            printf("New Allergies (Enter to keep '%s'): ", p->allergies);
            safeInput(tmp, sizeof(tmp)); if (strlen(tmp) > 0) strcpy(p->allergies, tmp);
            
            saveDatabase(db);
            writeAuditLog(emp->id, "UPDATED_PATIENT", p->id);
            printf("\n[SUCCESS] Patient comprehensive details updated.\n");
            pauseSystem();
        }
        else if (choice == 5) {
            Patient *found_pat = searchPatient(db);
            if (found_pat) displayPatientHistory(db, found_pat);
        }
        else if (choice == 6) {
            printf("\n--- ALL ACTIVE PATIENTS ---\n");
            printf("  %-10s | %-20s | %-5s | %-12s | %-15s\n", "ID", "Name", "Age", "Gender", "Contact");
            printf("  -----------------------------------------------------------------------\n");
            int count = 0;
            for (int i = 0; i < db->pat_count; i++) {
                Patient *p = &db->patients[i];
                if (p->is_active) {
                    printf("  %-10s | %-20s | %-5d | %-12s | %-15s\n",
                        p->id, p->name, p->age, p->gender, p->contact);
                    count++;
                    if (count % 15 == 0) {
                        pauseSystem();
                        printHeader("ALL ACTIVE PATIENTS (Continued)");
                        printf("  %-10s | %-20s | %-5s | %-12s | %-15s\n", "ID", "Name", "Age", "Gender", "Contact");
                        printf("  -----------------------------------------------------------------------\n");
                    }
                }
            }
            if (count == 0) printf("  No active patients.\n");
            else printf("\n  Total: %d active patient(s).\n", count);
            pauseSystem();
        }
        else if (choice == 7) {
            printf("\n--- PENDING CHECKOUTS & QUEUE MANAGEMENT ---\n");
            int found = 0;
            for(int i=0; i<db->visit_count; i++) {
                if((db->visits[i].status == REQ_BILL || db->visits[i].status == REQ_DOC || db->visits[i].status == REQ_LAB) && !db->visits[i].is_paid) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if(p) { printf("Visit ID: %s | Status Enum: %d | Patient: %s\n", db->visits[i].visit_id, db->visits[i].status, p->name); found = 1; }
                }
            }
            if(!found) { printf("Queue is empty.\n"); pauseSystem(); continue; }

            char v_id[ID_LEN];
            printf("\nEnter Visit ID to Process or Cancel: "); safeInput(v_id, ID_LEN);
            for(int i=0; i<db->visit_count; i++) {
                if(strcmp(db->visits[i].visit_id, v_id) == 0) {
                    
                    printf("\nAction: (1) Process Bill, (2) Cancel/Abort Visit: ");
                    if(getValidInt(1, 2) == 2) {
                        db->visits[i].status = CANCELLED_BY_PATIENT;
                        saveDatabase(db);
                        writeAuditLog(emp->id, "CANCELLED_VISIT", v_id);
                        printf("[SYSTEM] Visit permanently cancelled.\n");
                        break;
                    }

                    if (db->visits[i].status != REQ_BILL) {
                        printf("[ERROR] Patient has not finished clinical routing yet.\n"); break;
                    }

                    // Pre-Billing Visibility Fix (Flaw 5)
                    printf("\n--- PRE-BILLING CLINICAL SUMMARY ---\n");
                    printf("Doctor's Advice: %s\n", db->visits[i].doc_advice);
                    if (strcmp(db->visits[i].tests_required, "None") != 0) {
                        printf("Tests Conducted: %s\n", db->visits[i].tests_required);
                        printf("Lab Results: %s\n", db->visits[i].test_results);
                    }
                    printf("------------------------------------\n");

                    printf("Doctor's Prescribed Medicines: %s\n", db->visits[i].medicines);
                    
                    printf("Any extra items to add? (Type 'None' to skip): ");
                    char extra_meds[100];
                    safeInput(extra_meds, 100);
                    if (custom_strcasestr(extra_meds, "none") == NULL) {
                        int remaining_space = 199 - strlen(db->visits[i].medicines);
                        if (remaining_space > 5) {
                            strncat(db->visits[i].medicines, ", ", remaining_space);
                            strncat(db->visits[i].medicines, extra_meds, remaining_space - 2);
                        }
                        printf("Updated Medicines List: %s\n", db->visits[i].medicines);
                    }

                    printf("Enter Total Pharmacy Cost ($): "); 
                    char buf[50]; safeInput(buf, 50); db->visits[i].med_cost = atof(buf);
                    
                    if (strcmp(db->visits[i].tests_required, "None") != 0) {
                        printf("Enter Lab Cost ($): "); 
                        safeInput(buf, 50); db->visits[i].test_cost = atof(buf);
                    } else { db->visits[i].test_cost = 0.0; }

                    double ward_cost = 0.0;
                    if (strcmp(db->visits[i].ward_bed, "Outpatient") != 0) {
                        printf("Patient admitted to %s. Enter Ward/Bed Charges ($): ", db->visits[i].ward_bed);
                        safeInput(buf, 50); ward_cost = atof(buf);
                    }
                    
                    db->visits[i].doc_fee = CONSULTATION_FEE + ward_cost;

                    exportInvoice(db, &db->visits[i]);
                    printf("\nCollect Payment from Patient? (1 = Yes, 0 = No): ");
                    if(getValidInt(0, 1) == 1) {
                        db->visits[i].is_paid = 1;
                        db->visits[i].status = COMPLETED; 
                        saveDatabase(db);
                        writeAuditLog(emp->id, "PROCESSED_PAYMENT", v_id);
                        printf("[SUCCESS] Bill Paid. Workflow Complete.\n");
                    }
                    break;
                }
            }
            pauseSystem();
        }
        else if (choice == 8) {
            char pId[ID_LEN];
            printf("\nEnter Patient ID to Deactivate/Discharge: "); safeInput(pId, ID_LEN);
            Patient *p = getPatientByID(db, pId);
            if (!p) { printf("\n[ERROR] Patient not found.\n"); pauseSystem(); continue; }
            
            printf("Are you sure you want to discharge/deactivate %s? (1=Yes, 0=No): ", p->name);
            if (getValidInt(0, 1) == 1) {
                p->is_active = 0;
                saveDatabase(db);
                writeAuditLog(emp->id, "DISCHARGED_PATIENT", p->id);
                printf("[SUCCESS] Patient marked as inactive.\n");
            }
            pauseSystem();
        }
        else if (choice == 9) { 
            printHeader("UPDATE MY PROFILE");
            printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Shift\nChoice: ");
            int ch = getValidInt(1, 3);
            
            if (ch == 1) {
                printf("Enter New Name: "); safeInput(emp->name, MAX_STR);
                printf("[SUCCESS] Name updated.\n");
            } else if (ch == 2) {
                char raw[45];
                printf("Enter New Password: "); safeInput(raw, sizeof(raw));
                encryptPasswordHex(raw, emp->password);
                printf("[SUCCESS] Password updated.\n");
            } else if (ch == 3) {
                printf("Enter New Shift: "); safeInput(emp->shift, 50);
                printf("[SUCCESS] Shift updated.\n");
            }
            saveDatabase(db);
            writeAuditLog(emp->id, "UPDATED_OWN_PROFILE", emp->id);
            pauseSystem();
        }
    } while (choice != 10);
}

// ==========================================
// 18. THE LAB REPORT EXPORT ENGINE
// ==========================================
void exportLabReport(Database *db, VisitRecord *v) {
    Patient *p = getPatientByID(db, v->patient_id);
    LabAssistant *l = NULL;
    for (int i = 0; i < db->lab_count; i++) {
        if (strcmp(db->lab_assts[i].id, v->lab_id) == 0 && db->lab_assts[i].is_active) {
            l = &db->lab_assts[i]; break;
        }
    }
    if (!p || !l) return;

    char filename[MAX_STR];
    formatFilename(filename, p->name, "_LabReport.txt");
    FILE *f = fopen(filename, "a"); // FIXED: Append Mode
    if (f) {
        fprintf(f, "\n\n*************************************************\n");
        fprintf(f, "               NEW LAB REPORT ENTRY              \n");
        char timeStr[50]; getCurrentTimestamp(timeStr);
        fprintf(f, "=================================================\n");
        fprintf(f, "         %s \n", CLINIC_NAME);
        fprintf(f, "         %s \n", CLINIC_ADDRESS);
        fprintf(f, "=================================================\n");
        fprintf(f, "Date: %s | Visit ID: %s\n", timeStr, v->visit_id);
        fprintf(f, "Patient: %-20s Phone: %s\n", p->name, p->contact); // Includes Phone
        fprintf(f, "Age: %-5d Gender: %-10s Blood: %s\n", p->age, p->gender, p->blood_group);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "TESTS REQUESTED (By Doctor):\n%s\n", v->tests_required);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "OFFICIAL TEST RESULTS:\n%s\n", v->test_results);
        fprintf(f, "=================================================\n");
        fprintf(f, "Processed By: %s (ID: %s)\n", l->name, l->id);
        fprintf(f, "Qualifications: %s\n", l->qualification);
        fprintf(f, "=================================================\n");
        fclose(f);
        printf("\n[SYSTEM] Exported Lab Report to: %s\n", filename);
    }
}

// ==========================================
// 20. THE REMAINING DASHBOARDS
// ==========================================
void labAssistantDashboard(Database *db, LabAssistant *lab) {
    int choice;
    do {
        printHeader("LAB DIAGNOSTICS DASHBOARD");
        printf("Welcome, %s\n\n", lab->name);
        printf("1. View Pending Tests (REQ_LAB)\n");
        printf("2. Input Test Results\n");
        printf("3. Search Old Patient & View History\n");
        printf("4. Update My Profile\n");
        printf("5. Logout\nChoice: ");
        choice = getValidInt(1, 5);

        if (choice == 1) {
            printf("\n--- PENDING LAB QUEUE ---\n");
            int found = 0;
            for (int i = 0; i < db->visit_count; i++) {
                if (db->visits[i].status == REQ_LAB) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if(p) {
                        printf("Visit ID: %s | Patient: %s | Tests: %s\n", db->visits[i].visit_id, p->name, db->visits[i].tests_required);
                        found = 1;
                    }
                }
            }
            if (!found) printf("No pending lab work.\n");
            pauseSystem();
        } 
        else if (choice == 2) {
            char v_id[ID_LEN];
            printf("\nEnter Visit ID: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && db->visits[i].status == REQ_LAB) {
                    v = &db->visits[i]; break;
                }
            }
            
            if (v) {
                printf("\n--- ENTER LAB RESULTS ---\n");
                printf("Requested Tests: %s\n", v->tests_required);
                printf("Enter Official Results: "); safeInput(v->test_results, 200);
                
                strcpy(v->lab_id, lab->id); // Log which assistant did the test
                v->status = REQ_BILL; // Route back to Employee for billing
                
                exportLabReport(db, v);
                saveDatabase(db);
                writeAuditLog(lab->id, "COMPLETED_LAB_TEST", v->visit_id);
                printf("\n[SUCCESS] Results saved. Routed to Billing Queue.\n");
            } else {
                printf("[ERROR] Visit ID not found or not in Lab Queue.\n");
            }
            pauseSystem();
        }
        else if (choice == 3) {
            Patient *found_pat = searchPatient(db);
            if (found_pat) displayPatientHistory(db, found_pat);
        }
        else if (choice == 4) { 
            printHeader("UPDATE MY PROFILE");
            printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Contact Phone\nChoice: ");
            int ch = getValidInt(1, 3);
            
            if (ch == 1) {
                printf("Enter New Name: "); safeInput(lab->name, MAX_STR);
                printf("[SUCCESS] Name updated.\n");
            } else if (ch == 2) {
                char raw[45];
                printf("Enter New Password: "); safeInput(raw, sizeof(raw));
                encryptPasswordHex(raw, lab->password);
                printf("[SUCCESS] Password updated.\n");
            } else if (ch == 3) {
                printf("Enter New Phone: "); safeInput(lab->contact, 20);
                printf("[SUCCESS] Phone updated.\n");
            }
            saveDatabase(db);
            writeAuditLog(lab->id, "UPDATED_OWN_PROFILE", lab->id);
            pauseSystem();
        }
    } while (choice != 5);
}

void patientDashboard(Database *db, Patient *pat) {
    int choice;
    do {
        printHeader("PATIENT SELF-SERVICE KIOSK");
        printf("Welcome, %s\n\n", pat->name);
        printf("1. View My Profile Details\n");
        printf("2. View My Complete Medical History & Lab Reports\n");
        printf("3. Check All Invoices (Paid & Pending)\n");
        printf("4. Update My Password\n");
        printf("5. View Clinic Information (About Us)\n");
        printf("6. Logout\nChoice: ");
        choice = getValidInt(1, 6);

        if (choice == 1) {
            printf("\n--- MY PROFILE ---\n");
            printf("ID: %s\nName: %s\nEmail: %s\nAge: %d\nGender: %s\nBlood Group: %s\nPhone: %s\nAllergies: %s\n",
                   pat->id, pat->name, pat->email, pat->age, pat->gender, pat->blood_group, pat->contact, pat->allergies);
            pauseSystem();
        }
        else if (choice == 2) {
            displayPatientHistory(db, pat); // Calls our new master function
        }
        else if (choice == 3) {
            printf("\n--- INVOICE HISTORY ---\n");
            int found = 0;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].patient_id, pat->id) == 0 && db->visits[i].status >= REQ_BILL) {
                    printf("Date: %s | Invoice ID: %s | Total: $%.2f | Status: %s\n", 
                           db->visits[i].date, db->visits[i].visit_id, db->visits[i].total_bill, 
                           db->visits[i].is_paid ? "[PAID]" : "[PENDING - Please see Reception]");
                    found = 1;
                }
            }
            if (!found) printf("No billing records found.\n");
            pauseSystem();
        }
        else if (choice == 3) {
            updateOwnPassword(pat->password);
            saveDatabase(db);
        }
        else if (choice == 4) {
            printHeader("ABOUT US");
            printf("Address: %s\n", CLINIC_ADDRESS);
            printf("Total Medical Professionals on Staff: %d\n", db->doc_count);
            printf("Emergency Contact: 555-0199\n");
            pauseSystem();
        }
    } while (choice != 6);
}



void adminDashboard(Database *db, Admin *admin) {
    int choice;
    do {
        printHeader("MASTER ADMIN DASHBOARD");
        printf("Welcome, %s\n\n", admin->name);
        printf("1. Register New Admin\n");
        printf("2. Register New Employee\n");
        printf("3. Register New Doctor\n");
        printf("4. Register New Lab Assistant\n");
        printf("5. Manage Directory (View/Delete with Pagination)\n");
        printf("6. Update User Details (Fix Typos)\n");
        printf("7. View System Logs (Audit)\n");
        printf("8. Logout\nChoice: ");
        choice = getValidInt(1, 8);

        if (choice == 1) registerAdmin(db);
        else if (choice == 2) registerEmployee(db, admin->id);
        else if (choice == 3) registerDoctor(db, admin->id);
        else if (choice == 4) registerLabAssistant(db, admin->id);
        else if (choice == 5) adminManageDirectory(db);
        else if (choice == 6) adminUpdateUser(db, admin->id);
        else if (choice == 7) {
            printf("\n--- SYSTEM AUDIT LOGS ---\n");
            FILE *f = fopen("audit_log.txt", "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) printf("%s", line);
                fclose(f);
            } else { printf("No logs found.\n"); }
            pauseSystem();
        }
    } while (choice != 8);
}

// ==========================================
// 21. THE FULLY INTEGRATED MAIN GATEWAY
// ==========================================
int main() {
    Database db;
    // Explicitly nullify all pointers to guarantee Valgrind safety
    db.admins = NULL; db.employees = NULL; db.doctors = NULL;
    db.lab_assts = NULL; db.patients = NULL; db.visits = NULL;
    
    // Explicitly zero capacities and counts
    db.admin_count = 0; db.admin_cap = 0; db.emp_count = 0; db.emp_cap = 0;
    db.doc_count = 0; db.doc_cap = 0; db.lab_count = 0; db.lab_cap = 0;
    db.pat_count = 0; db.pat_cap = 0; db.visit_count = 0; db.visit_cap = 0;

    loadDatabase(&db);

    // FIRST BOOT PARADOX
    if (db.admin_count == 0) {
        printHeader("SYSTEM INITIALIZATION");
        printf("Database empty. First boot detected.\n");
        registerAdmin(&db); 
    }

    int role;
    do {
        printHeader("ENTERPRISE CLINIC MANAGEMENT");
        printf("1. Admin Login\n");
        printf("2. Doctor Login\n");
        printf("3. Employee (Reception) Login\n");
        printf("4. Lab Assistant Login\n");
        printf("5. Patient Self-Service Kiosk\n");
        printf("6. New Patient Registration\n");
        printf("7. Forgot Password Recovery\n");
        printf("8. Exit & Shutdown Safely\n\n");
        printf("Select Portal: ");
        role = getValidInt(1, 8);

        if (role == 1) {
            Admin *a = loginAdmin(&db);
            if (a) adminDashboard(&db, a);
        } else if (role == 2) {
            Doctor *d = loginDoctor(&db);
            if (d) doctorDashboard(&db, d);
        } else if (role == 3) {
            char em[MAX_STR], pw[MAX_STR], epw[MAX_STR];
            printHeader("RECEPTION PORTAL");
            printf("Email: "); safeInput(em, MAX_STR);
            printf("Password: "); safeInput(pw, MAX_STR);
            
            // FIXED: Now correctly matches the Hex-XOR cipher used in registration
            encryptPasswordHex(pw, epw); 
            
            Employee *e = NULL;
            for(int i = 0; i < db.emp_count; i++) {
                if(db.employees[i].is_active && strcmp(db.employees[i].email, em) == 0 && strcmp(db.employees[i].password, epw) == 0) {
                    e = &db.employees[i];
                }
            }
            
            if (e) employeeDashboard(&db, e);
            else { printf("\n[ERROR] Invalid Credentials.\n"); pauseSystem(); }
        } else if (role == 4) {
            LabAssistant *l = loginLabAssistant(&db);
            if (l) labAssistantDashboard(&db, l);
        } else if (role == 5) {
            Patient *p = loginPatient(&db);
            if (p) patientDashboard(&db, p);
        } else if (role == 6) {
            registerPatient(&db, "SELF_REGISTER");
        } else if (role == 7) {
            forgotPasswordRecovery(&db);
        } else if (role == 8) {
            printf("\n[SYSTEM] Commencing safe shutdown...\n");
            saveDatabase(&db);
            freeDatabase(&db); 
            printf("[SYSTEM] Memory cleared. Goodbye.\n");
        }
    } while (role != 8);

    return 0;
}