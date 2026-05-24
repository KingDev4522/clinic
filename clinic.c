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
    char specialization[MAX_STR]; // NEW: Specialization for Lab Assitant
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
    int ward_days;      // Explicitly store days stayed
    int is_discharged;  // Flag to indicate discharge status
    int ward_doc_visits; // NEW: Number of doc visits in ward
    int direct_admission; // NEW: Flag for direct emergency admit (waives first fee)
} VisitRecord;

// Standalone Pharmacy Database
typedef struct {
    char sale_id[ID_LEN];
    char patient_id[ID_LEN]; // "WALK-IN" or Patient ID
    char name[MAX_STR];
    char contact[20];
    char email[MAX_STR];     // "NA" for walk-ins
    char medicines[200];
    double cost;
    char date[30];
} PharmacySale;

// The Master RAM Database
typedef struct {
    Admin *admins;              int admin_count;    int admin_cap;
    Employee *employees;        int emp_count;      int emp_cap;
    Doctor *doctors;            int doc_count;      int doc_cap;
    LabAssistant *lab_assts;    int lab_count;      int lab_cap;
    Patient *patients;          int pat_count;      int pat_cap;
    VisitRecord *visits;        int visit_count;    int visit_cap;
    PharmacySale *pharmacy_sales; int pharm_count;  int pharm_cap;
} Database;

// ==========================================
// FORWARD DECLARATIONS (Fixes Implicit Declaration Warnings)
// ==========================================
void addAdminToDB(Database *db, Admin a);
void addEmployeeToDB(Database *db, Employee e);
void addDoctorToDB(Database *db, Doctor d);
void addLabAsstToDB(Database *db, LabAssistant l);
void addPatientToDB(Database *db, Patient p);
void addVisitToDB(Database *db, VisitRecord v);
void addPharmacySaleToDB(Database *db, PharmacySale ps);
void viewPharmacyDatabase(Database *db);

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

// SAFE HEX-XOR DECRYPTION: Reverses the cipher for Admin viewing
void decryptPasswordHex(const char* input, char* output) {
    char key = 'K';
    int i, j = 0;
    char hexByte[3] = {0}; // Buffer to hold 2 hex characters + null terminator
    
    for (i = 0; input[i] != '\0' && input[i+1] != '\0'; i += 2) {
        hexByte[0] = input[i];
        hexByte[1] = input[i+1];
        unsigned int val;
        sscanf(hexByte, "%X", &val); // Convert Hex string back to integer
        output[j++] = (char)(val ^ key); // Reverse the XOR
    }
    output[j] = '\0';
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
            
            char* token = strtok(tempLine, "|"); 
            if (token != NULL) {
                // FIX: visits_db.txt does NOT start with isActive. The first token IS the ID.
                // Other databases start with isActive, so their ID is the second token.
                if (strcmp(filename, "visits_db.txt") != 0) {
                    token = strtok(NULL, "|"); // Grab the ID token for non-visit DBs
                }
                
                if (token != NULL) {
                    // FIXED: Robust string parsing replacing fragile sscanf
                    char* slash = strchr(token, '/');
                    if (slash != NULL) {
                        int num = atoi(slash + 1); // Directly converts everything after '/' to an int
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
    if (db->pharmacy_sales) free(db->pharmacy_sales);

    db->admins = NULL; db->employees = NULL; db->doctors = NULL;
    db->lab_assts = NULL; db->patients = NULL; db->visits = NULL;
    db->pharmacy_sales = NULL;

    db->admin_count = 0; db->emp_count = 0; db->doc_count = 0;
    db->lab_count = 0; db->pat_count = 0; db->visit_count = 0;
    db->pharm_count = 0;
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

void addPharmacySaleToDB(Database *db, PharmacySale ps) {
    if (db->pharm_count >= db->pharm_cap) {
        int new_cap = (db->pharm_cap == 0) ? 10 : db->pharm_cap * 2;
        PharmacySale *temp = (PharmacySale *)realloc(db->pharmacy_sales, new_cap * sizeof(PharmacySale));
        if (!temp) {
            printf("\n[FATAL ERROR] Out of memory. Cannot add Pharmacy Sale.\n");
            return;
        }
        db->pharmacy_sales = temp;
        db->pharm_cap = new_cap;
    }
    db->pharmacy_sales[db->pharm_count++] = ps;
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
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(l.specialization, token); // NEW
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
            // Safe Backward-Compatible loading for old database files
            token = parseDelimitedString(&rest, "|"); if (token) v.ward_days = atoi(token); else v.ward_days = 0;
            token = parseDelimitedString(&rest, "|"); if (token) v.is_discharged = atoi(token); else v.is_discharged = 0;
            token = parseDelimitedString(&rest, "|"); if (token) v.ward_doc_visits = atoi(token); else v.ward_doc_visits = 0;
            token = parseDelimitedString(&rest, "|"); if (token) v.direct_admission = atoi(token); else v.direct_admission = 0;
            addVisitToDB(db, v);
        } fclose(fVis);
    }

    FILE *fPharm = fopen("pharmacy_db.txt", "r");
    if (fPharm) {
        while (fgets(line, sizeof(line), fPharm)) {
            PharmacySale ps = {0}; rest = line;
            token = parseDelimitedString(&rest, "|"); if (!token) continue; strcpy(ps.sale_id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(ps.patient_id, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(ps.name, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(ps.contact, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(ps.email, token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(ps.medicines, token);
            token = parseDelimitedString(&rest, "|"); if (token) ps.cost = atof(token);
            token = parseDelimitedString(&rest, "|"); if (token) strcpy(ps.date, token);
            addPharmacySaleToDB(db, ps);
        } fclose(fPharm);
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
            fprintf(fLab, "%d|%s|%s|%s|%s|%s|%s|%s\n", db->lab_assts[i].is_active, db->lab_assts[i].id, db->lab_assts[i].name, db->lab_assts[i].email, db->lab_assts[i].password, db->lab_assts[i].qualification, db->lab_assts[i].specialization, db->lab_assts[i].contact);
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
            fprintf(fVis, "%s|%s|%s|%s|%d|%s|%s|%s|%s|%s|%s|%s|%s|%s|%.2f|%.2f|%.2f|%.2f|%d|%d|%d|%d|%d\n",
                    v.visit_id, v.patient_id, v.doc_id, v.lab_id, v.status, v.date, v.ward_bed,
                    v.vitals, v.symptoms, v.diagnosis, v.medicines, v.tests_required,
                    v.test_results, v.doc_advice, v.doc_fee, v.med_cost, v.test_cost,
                    v.total_bill, v.is_paid, v.ward_days, v.is_discharged, v.ward_doc_visits, v.direct_admission);
        }
        fclose(fVis);
    }

    FILE *fPharm = fopen("pharmacy_db.txt", "w");
    if (fPharm) {
        for (int i = 0; i < db->pharm_count; i++) {
            fprintf(fPharm, "%s|%s|%s|%s|%s|%s|%.2f|%s\n",
                    db->pharmacy_sales[i].sale_id, db->pharmacy_sales[i].patient_id,
                    db->pharmacy_sales[i].name, db->pharmacy_sales[i].contact,
                    db->pharmacy_sales[i].email, db->pharmacy_sales[i].medicines,
                    db->pharmacy_sales[i].cost, db->pharmacy_sales[i].date);
        }
        fclose(fPharm);
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

// NEW: Calculate days stayed based on admission date string
int calculateDaysStayed(const char *admission_date) {
    struct tm adm_tm = {0};
    int day, month, year, h, m, s;
    if (sscanf(admission_date, "%d-%d-%d %d:%d:%d", &day, &month, &year, &h, &m, &s) == 6) {
        adm_tm.tm_mday = day;
        adm_tm.tm_mon = month - 1;
        adm_tm.tm_year = year - 1900;
        adm_tm.tm_hour = h;
        adm_tm.tm_min = m;
        adm_tm.tm_sec = s;
        time_t adm_time = mktime(&adm_tm);
        time_t current_time = time(NULL);
        double diff_seconds = difftime(current_time, adm_time);
        int days = (int)(diff_seconds / (60 * 60 * 24));
        return (days < 1) ? 1 : days; // Minimum charge is 1 day
    }
    return 1;
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
    int keep_adding = 1;
    while(keep_adding) {
        Admin a = {0}; a.is_active = 1;
        generateUniqueID("admin_db.txt", "ADM", a.id);
        
        printf("\n--- REGISTER MASTER ADMIN ---\n");
        printf("Admin ID Auto-Assigned: %s\n", a.id);
        printf("Enter Full Name: "); safeInput(a.name, MAX_STR);
        printf("Enter Email Address: "); safeInput(a.email, MAX_STR);
        
        if (!isEmailUnique(db, a.email)) {
            printf("\n[ERROR] Email already exists in the system. Registration aborted.\n");
        } else {
            char rawPass[45] = "123";
            printf("Temporary Password set to: 123 (User must change this later)\n"); 
            encryptPasswordHex(rawPass, a.password); 
            
            addAdminToDB(db, a); saveDatabase(db);
            writeAuditLog(a.id, "REGISTERED_NEW_ADMIN", a.id);
            printf("\n[SUCCESS] Master Admin Registered.\n"); 
        }
        printf("\nDo you want to add another Admin? (1 = Yes, 0 = No): ");
        keep_adding = getValidInt(0, 1);
    }
}

void registerEmployee(Database *db, const char* admin_id) {
    int keep_adding = 1;
    while(keep_adding) {
        Employee e = {0}; e.is_active = 1;
        generateUniqueID("emp_db.txt", "EMP", e.id);
        
        printf("\n--- REGISTER NEW EMPLOYEE ---\n");
        printf("Employee ID Auto-Assigned: %s\n", e.id);
        printf("Enter Full Name: "); safeInput(e.name, MAX_STR);
        printf("Enter Email Address: "); safeInput(e.email, MAX_STR);
        
        if (!isEmailUnique(db, e.email)) {
            printf("\n[ERROR] Email already exists. Registration aborted.\n");
        } else {
            char rawPass[45] = "123";
            printf("Temporary Password set to: 123 (User must change this later)\n"); 
            encryptPasswordHex(rawPass, e.password); 
            
            printf("Enter Assigned Shift (e.g., Morning/Night): "); safeInput(e.shift, 50);
            printf("Enter Official Designation (e.g., Sr. Receptionist): "); safeInput(e.designation, 50);
            
            addEmployeeToDB(db, e); saveDatabase(db);
            writeAuditLog(admin_id, "REGISTERED_EMPLOYEE", e.id);
            printf("\n[SUCCESS] Employee Registered Successfully.\n"); 
        }
        printf("\nDo you want to add another Employee? (1 = Yes, 0 = No): ");
        keep_adding = getValidInt(0, 1);
    }
}

void registerDoctor(Database *db, const char* admin_id) {
    int keep_adding = 1;
    while(keep_adding) {
        Doctor d = {0}; d.is_active = 1;
        generateUniqueID("doc_db.txt", "DOC", d.id);
        
        printf("\n--- REGISTER NEW DOCTOR ---\n");
        printf("Doctor ID Auto-Assigned: %s\n", d.id);
        printf("Enter Full Name (with Dr.): "); safeInput(d.name, MAX_STR);
        printf("Enter Email Address: "); safeInput(d.email, MAX_STR);
        
        if (!isEmailUnique(db, d.email)) {
            printf("\n[ERROR] Email already exists. Registration aborted.\n");
        } else {
            char rawPass[45] = "123";
            printf("Temporary Password set to: 123 (User must change this later)\n"); 
            encryptPasswordHex(rawPass, d.password); 
            
            printf("Enter Qualifications (e.g., MBBS, MD): "); safeInput(d.qualification, MAX_STR);
            printf("Enter Specialization (e.g., Orthopedics, General): "); safeInput(d.specialization, MAX_STR);
            printf("Enter Assigned Room Number: "); safeInput(d.room_number, 10);
            printf("Enter Doctor's Contact Phone: "); safeInput(d.contact, 20);
            
            addDoctorToDB(db, d); saveDatabase(db);
            writeAuditLog(admin_id, "REGISTERED_DOCTOR", d.id);
            printf("\n[SUCCESS] Doctor Registered Successfully.\n"); 
        }
        printf("\nDo you want to add another Doctor? (1 = Yes, 0 = No): ");
        keep_adding = getValidInt(0, 1);
    }
}

void registerLabAssistant(Database *db, const char* admin_id) {
    int keep_adding = 1;
    while(keep_adding) {
        LabAssistant l = {0}; l.is_active = 1;
        generateUniqueID("lab_db.txt", "LAB", l.id);
        printf("\n--- REGISTER NEW LAB ASSISTANT ---\n");
        printf("Lab Assistant ID Auto-Assigned: %s\n", l.id);
        printf("Enter Full Name: "); safeInput(l.name, MAX_STR);
        printf("Enter Email Address: "); safeInput(l.email, MAX_STR);
        
        if (!isEmailUnique(db, l.email)) {
            printf("\n[ERROR] Email exists.\n"); 
        } else {
            char rawPass[45] = "123";
            printf("Temporary Password set to: 123 (User must change this later)\n"); 
            encryptPasswordHex(rawPass, l.password); 
            
            printf("Enter Qualifications (e.g., BSc, DMLT): "); safeInput(l.qualification, MAX_STR);
            printf("Enter Specialization (e.g., Blood Test, ECG, MRI, USG): "); safeInput(l.specialization, MAX_STR);
            printf("Enter Contact Phone Number: "); safeInput(l.contact, 20);
            
            addLabAsstToDB(db, l); saveDatabase(db);
            writeAuditLog(admin_id, "REGISTERED_LAB_ASST", l.id);
            printf("\n[SUCCESS] Lab Assistant Registered.\n"); 
        }
        printf("\nDo you want to add another Lab Assistant? (1 = Yes, 0 = No): ");
        keep_adding = getValidInt(0, 1);
    }
}

void registerPatient(Database *db, const char* emp_id) {
    int keep_adding = 1;
    while(keep_adding) {
        Patient p = {0}; p.is_active = 1;
        generateUniqueID("patient_db.txt", "P26", p.id);
        printf("\n--- PATIENT INTAKE & REGISTRATION ---\n");
        printf("Auto-Assigned ID: %s\n", p.id);
        printf("Name: "); safeInput(p.name, MAX_STR);
        printf("Email: "); safeInput(p.email, MAX_STR);
        
        if(!isEmailUnique(db, p.email)) { 
            printf("[ERROR] Email in use.\n"); 
        } else {
            char rawPass[45] = "123";
            printf("Temporary Portal Password set to: 123 (User must change this later)\n"); 
            encryptPasswordHex(rawPass, p.password);
            
            printf("Age: "); p.age = getValidInt(0, 120);
            printf("Gender: "); safeInput(p.gender, 15);
            printf("Blood Group: "); safeInput(p.blood_group, 10);
            printf("Contact Phone Number: "); safeInput(p.contact, 20); 
            printf("Known Allergies (CRITICAL - Type 'None' if none): "); safeInput(p.allergies, MAX_STR);
            
            addPatientToDB(db, p); saveDatabase(db);
            writeAuditLog(emp_id, "REGISTERED_PATIENT", p.id);
            printf("\n[SUCCESS] Patient Registration Complete.\n"); 
        }
        printf("\nDo you want to add another Patient? (1 = Yes, 0 = No): ");
        keep_adding = getValidInt(0, 1);
    }
}

// Universal Password Recovery (Handles all roles based on role_type)
void forgotPasswordRecovery(Database *db, int role_type) {
    char email[MAX_STR], verify[MAX_STR];
    printHeader("FORGOT PASSWORD RECOVERY");
    printf("Enter your registered Email: "); safeInput(email, MAX_STR);

    int found = 0;
    char *passPtr = NULL, *namePtr = NULL, *idPtr = NULL;

    // Role 1=Admin, 2=Doctor, 3=Employee, 4=LabAsst, 5=Patient
    if (role_type == 1) {
        printf("Enter Admin ID for verification: "); safeInput(verify, MAX_STR);
        for (int i=0; i<db->admin_count; i++) if (db->admins[i].is_active && strcmp(db->admins[i].email, email)==0 && strcmp(db->admins[i].id, verify)==0) { found=1; passPtr=db->admins[i].password; namePtr=db->admins[i].name; idPtr=db->admins[i].id; break; }
    } else if (role_type == 2) {
        printf("Enter registered Phone Number for verification: "); safeInput(verify, MAX_STR);
        for (int i=0; i<db->doc_count; i++) if (db->doctors[i].is_active && strcmp(db->doctors[i].email, email)==0 && strcmp(db->doctors[i].contact, verify)==0) { found=1; passPtr=db->doctors[i].password; namePtr=db->doctors[i].name; idPtr=db->doctors[i].id; break; }
    } else if (role_type == 3) {
        printf("Enter Employee ID for verification: "); safeInput(verify, MAX_STR);
        for (int i=0; i<db->emp_count; i++) if (db->employees[i].is_active && strcmp(db->employees[i].email, email)==0 && strcmp(db->employees[i].id, verify)==0) { found=1; passPtr=db->employees[i].password; namePtr=db->employees[i].name; idPtr=db->employees[i].id; break; }
    } else if (role_type == 4) {
        printf("Enter registered Phone Number for verification: "); safeInput(verify, MAX_STR);
        for (int i=0; i<db->lab_count; i++) if (db->lab_assts[i].is_active && strcmp(db->lab_assts[i].email, email)==0 && strcmp(db->lab_assts[i].contact, verify)==0) { found=1; passPtr=db->lab_assts[i].password; namePtr=db->lab_assts[i].name; idPtr=db->lab_assts[i].id; break; }
    } else if (role_type == 5) {
        printf("Enter registered Phone Number for verification: "); safeInput(verify, MAX_STR);
        for (int i=0; i<db->pat_count; i++) if (db->patients[i].is_active && strcmp(db->patients[i].email, email)==0 && strcmp(db->patients[i].contact, verify)==0) { found=1; passPtr=db->patients[i].password; namePtr=db->patients[i].name; idPtr=db->patients[i].id; break; }
    }

    if (found) {
        printf("\n[IDENTITY VERIFIED] Account found: %s\n", namePtr);
        char rawPass[45];
        printf("Enter NEW Password: "); safeInput(rawPass, sizeof(rawPass));
        encryptPasswordHex(rawPass, passPtr);
        saveDatabase(db);
        writeAuditLog(idPtr, "PASSWORD_RESET", "LOGIN_SCREEN");
        printf("[SUCCESS] Password updated. You may now log in.\n");
    } else {
        printf("\n[ERROR] Identity verification failed. Details do not match.\n");
    }
    pauseSystem();
}

// ==========================================
// AUTHENTICATION ENGINES
// ==========================================
Admin* loginAdmin(Database *db) {
    char email[MAX_STR], pass[MAX_STR], encPass[MAX_STR];
    printHeader("MASTER ADMIN SECURE LOGIN");
    printf("Enter Email: "); safeInput(email, MAX_STR);
    
    printf("Enter Password (or type 'FORGOT' | Temp Pass: 123): "); safeInput(pass, MAX_STR);
    if (strcmp(pass, "FORGOT") == 0) { forgotPasswordRecovery(db, 1); return NULL; }
    
    encryptPasswordHex(pass, encPass); 
    
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
    
    printf("Enter Password (or type 'FORGOT' | Temp Pass: 123): "); safeInput(pass, MAX_STR);
    if (strcmp(pass, "FORGOT") == 0) { forgotPasswordRecovery(db, 2); return NULL; }
    
    encryptPasswordHex(pass, encPass); 
    
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
    
    printf("Enter Password (or type 'FORGOT' | Temp Pass: 123): "); safeInput(pass, MAX_STR);
    if (strcmp(pass, "FORGOT") == 0) { forgotPasswordRecovery(db, 4); return NULL; }
    
    encryptPasswordHex(pass, encPass); 
    
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
    
    // UPDATED: Clear hint added for temporary password mapping
    printf("Enter Password (or type 'FORGOT' | Temp Pass: 123): "); safeInput(pass, MAX_STR);
    if (strcmp(pass, "FORGOT") == 0) { forgotPasswordRecovery(db, 5); return NULL; }
    
    encryptPasswordHex(pass, encPass); 
    
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
    
    // Write new entry to a temporary file first
    FILE *f = fopen("temp_rx.tmp", "w");
    if (f) {
        fprintf(f, "*************************************************\n");
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
        fprintf(f, "[VITALS] %s\n\n", v->vitals);
        fprintf(f, "SYMPTOMS:  %s\n", v->symptoms);
        fprintf(f, "DIAGNOSIS: %s\n\n", v->diagnosis);
        fprintf(f, "Rx (MEDICINES):\n%s\n\n", v->medicines);
        fprintf(f, "ADVICE:\n%s\n", v->doc_advice);
        if (v->status == REQ_LAB) fprintf(f, "\n*TESTS ADVISED: %s\n", v->tests_required);
        if (v->doc_fee > 0) fprintf(f, "\nCONSULTATION FEE (Billed): $%.2f\n", v->doc_fee);
        fprintf(f, "=================================================\n\n");

        // Copy old contents below the new entry
        FILE *oldF = fopen(filename, "r");
        if (oldF) {
            char ch;
            while ((ch = fgetc(oldF)) != EOF) {
                fputc(ch, f);
            }
            fclose(oldF);
        }
        fclose(f);
        
        // Replace old file with the new prepended file
        remove(filename);
        rename("temp_rx.tmp", filename);
        printf("\n[SYSTEM] Exported Prescription to: %s (Newest on top)\n", filename);
    }
}

void exportInvoice(Database *db, VisitRecord *v) {
    Patient *p = getPatientByID(db, v->patient_id);
    Doctor *d = getDoctorByID(db, v->doc_id); // NEW
    LabAssistant *l = NULL; // NEW
    for(int i=0; i<db->lab_count; i++) {
        if(strcmp(db->lab_assts[i].id, v->lab_id) == 0) { l = &db->lab_assts[i]; break; }
    }

    if (!p) return;

    char filename[MAX_STR];
    formatFilename(filename, p->name, "_Bill.txt");
    
    FILE *f = fopen("temp_bill.tmp", "w");
    if (f) {
        fprintf(f, "*************************************************\n");
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
        if (d) fprintf(f, "Attending Doctor: Dr. %s (%s)\n", d->name, d->specialization);
        if (l) fprintf(f, "Assigned Lab Tech: %s (%s)\n", l->name, l->specialization);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "DESCRIPTION                     AMOUNT ($)       \n");
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "Consultation & Ward Fees        %10.2f\n", v->doc_fee);
        fprintf(f, "Pharmacy / Medicines            %10.2f\n", v->med_cost);
        fprintf(f, "Laboratory Tests                %10.2f\n", v->test_cost);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "SUBTOTAL                        %10.2f\n", subtotal);
        fprintf(f, "TAX (5%%)                        %10.2f\n", tax);
        fprintf(f, "=================================================\n");
        fprintf(f, "GRAND TOTAL                     %10.2f\n", v->total_bill);
        fprintf(f, "STATUS: %s\n", v->is_paid ? "[ PAID ]" : "[ UNPAID ]");
        fprintf(f, "=================================================\n\n");

        FILE *oldF = fopen(filename, "r");
        if (oldF) {
            char ch;
            while ((ch = fgetc(oldF)) != EOF) {
                fputc(ch, f);
            }
            fclose(oldF);
        }
        fclose(f);
        
        remove(filename);
        rename("temp_bill.tmp", filename);
        printf("\n[SYSTEM] Exported Invoice to: %s (Newest on top)\n", filename);
    }
}


// ==========================================
// 15.5 CRUD & UNIVERSAL SEARCH ENGINES
// ==========================================

Patient* searchPatient(Database *db) {
    printf("\n--- SEARCH PATIENT DIRECTORY ---\n");
    
    // --- NEW ADDITION: Show old patients with visit history before prompt ---
    printf("\n--- PATIENTS WITH PREVIOUS VISIT HISTORY ---\n");
    int count = 0;
    for (int i = 0; i < db->pat_count; i++) {
        if (db->patients[i].is_active) {
            int has_history = 0;
            // Check if patient has any completed visits
            for (int j = 0; j < db->visit_count; j++) {
                if (strcmp(db->visits[j].patient_id, db->patients[i].id) == 0 && db->visits[j].status == COMPLETED) {
                    has_history = 1;
                    break;
                }
            }
            if (has_history) {
                printf("  ID: %-10s | Name: %-15s | Phone: %-15s | Email: %s\n", 
                       db->patients[i].id, db->patients[i].name, db->patients[i].contact, db->patients[i].email);
                count++;
            }
        }
    }
    if (count == 0) {
        printf("  [INFO] No old patients with visit history found.\n");
    }
    printf("--------------------------------------------\n\n");
    // --- END NEW ADDITION ---

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
            if (d) {
                printf("Attending Physician: Dr. %s | Qual: %s | Room: %s\n", d->name, d->qualification, d->room_number);
            } else {
                printf("Attending Physician: Unknown\n");
            }
            printf("Vitals: %s\n", db->visits[i].vitals);
            printf("Diagnosis: %s\n", db->visits[i].diagnosis);
            printf("Prescription: %s\n", db->visits[i].medicines);
            if (strcmp(db->visits[i].tests_required, "None") != 0) {
                printf("Lab Tests: %s | Results: %s\n", db->visits[i].tests_required, db->visits[i].test_results);
            // NEW: Relational lookup to pull the Lab Assistant's profile via lab_id
                LabAssistant *lab_asst = NULL;
                for (int j = 0; j < db->lab_count; j++) {
                    if (strcmp(db->lab_assts[j].id, db->visits[i].lab_id) == 0) {
                        lab_asst = &db->lab_assts[j];
                        break;
                    }
                }
                
                // Print the details if a valid Lab Assistant was found
                if (lab_asst) {
                    printf("Processed By (Lab): %s | Qual: %s | Spec: %s\n", lab_asst->name, lab_asst->qualification, lab_asst->specialization);
                } else {
                    printf("Processed By (Lab): Unknown\n");
                }
            }
            printf("--------------------------------------------------------\n");
            found = 1;
        }
    }
    if (!found) printf("No prior completed visits found for this patient.\n");
    pauseSystem();
}

void updateOwnPassword(char* encryptedPassStr) {
    char plainPass[MAX_STR];
    decryptPasswordHex(encryptedPassStr, plainPass); // DECRYPT CURRENT
    
    char raw[45];
    printf("\n--- UPDATE PROFILE PASSWORD ---\n");
    printf("Current Password: %s\n", plainPass);     // SHOW CURRENT
    printf("Enter New Password: "); safeInput(raw, sizeof(raw));
    
    encryptPasswordHex(raw, encryptedPassStr);
    printf("[SUCCESS] Password updated successfully.\n");
    pauseSystem();
}

void adminManageDirectory(Database *db) {
    printHeader("DIRECTORY MANAGEMENT & PAGINATION");
    printf("View Directory: (1) Doctors, (2) Employees, (3) Lab Assts, (4) Patients, (5) Admins\nChoice: ");
    int type = getValidInt(1, 5);
    int count = 0, total = 0;
    
    char plainPass[MAX_STR]; // NEW: Buffer to hold the decrypted password for display
    
    printf("\n--- ACTIVE DIRECTORY (Showing 15 per page) ---\n");
    if (type == 1) {
        total = db->doc_count;
        for (int i = 0; i < total; i++) {
            if (db->doctors[i].is_active) {
                decryptPasswordHex(db->doctors[i].password, plainPass); // NEW: Decrypt on-the-fly
                printf("[%d] ID: %s | Name: Dr. %s | Spec: %s | Email: %s | Pass: %s\n", 
                       i, db->doctors[i].id, db->doctors[i].name, db->doctors[i].specialization, db->doctors[i].email, plainPass);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    } else if (type == 2) {
        total = db->emp_count;
        for (int i = 0; i < total; i++) {
            if (db->employees[i].is_active) {
                decryptPasswordHex(db->employees[i].password, plainPass); // NEW: Decrypt on-the-fly
                printf("[%d] ID: %s | Name: %s | Desig: %s | Email: %s | Pass: %s\n", 
                       i, db->employees[i].id, db->employees[i].name, db->employees[i].designation, db->employees[i].email, plainPass);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    } else if (type == 3) {
        total = db->lab_count;
        for (int i = 0; i < total; i++) {
            if (db->lab_assts[i].is_active) {
                decryptPasswordHex(db->lab_assts[i].password, plainPass); 
                printf("[%d] ID: %s | Name: %s | Qual: %s | Spec: %s | Email: %s | Pass: %s\n", 
                       i, db->lab_assts[i].id, db->lab_assts[i].name, db->lab_assts[i].qualification, db->lab_assts[i].specialization, db->lab_assts[i].email, plainPass);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    } else if (type == 4) {
        total = db->pat_count;
        for (int i = 0; i < total; i++) {
            // REMOVED 'is_active' CHECK: Shows all patients universally
            decryptPasswordHex(db->patients[i].password, plainPass); 
            
            // NEW: Determine if patient is actively undergoing a visit
            char visit_status[20] = "Not Active";
            for (int j = 0; j < db->visit_count; j++) {
                if (strcmp(db->visits[j].patient_id, db->patients[i].id) == 0 && db->visits[j].status < COMPLETED) {
                    strcpy(visit_status, "Active");
                    break;
                }
            }

            printf("[%d] ID: %s | Name: %s | Age: %d | Blood: %s | Phone: %s | Allergies: %s | Pass: %s | Acc Status: %s | Visit Status: %s\n", 
                   i, db->patients[i].id, db->patients[i].name, db->patients[i].age, db->patients[i].blood_group, db->patients[i].contact, db->patients[i].allergies, plainPass, db->patients[i].is_active ? "Active" : "Deleted", visit_status);
            count++; if (count % 15 == 0) pauseSystem();
        }
    }else if (type == 5) {
        total = db->admin_count;
        for (int i = 0; i < total; i++) {
            if (db->admins[i].is_active) {
                decryptPasswordHex(db->admins[i].password, plainPass); 
                printf("[%d] ID: %s | Name: %s | Email: %s | Pass: %s\n", 
                       i, db->admins[i].id, db->admins[i].name, db->admins[i].email, plainPass);
                count++; if (count % 15 == 0) pauseSystem();
            }
        }
    }

    // --- REQUIREMENT 2: EMPTY STATE HANDLING ---
    if (count == 0) {
        printf("\n[INFO] No active users found in this directory.\n");
        pauseSystem();
        return; // Safely exit back to the dashboard loop
    }

    // NEW: Dynamically update prompt options based on what directory we are in
    if (type == 4) {
        printf("\nActions: (1) Soft Delete User, (2) Return to Dashboard, (3) View Patient Details\nChoice: ");
    } else {
        printf("\nActions: (1) Soft Delete User, (2) Return to Dashboard\nChoice: ");
    }
    
    int action = (type == 4) ? getValidInt(1, 3) : getValidInt(1, 2);

    if (action == 1) {
        char target_id[ID_LEN];
        printf("Enter ID to deactivate: "); safeInput(target_id, ID_LEN);
        int success = 0;
        if (type == 1) { for (int i = 0; i < total; i++) if (strcmp(db->doctors[i].id, target_id) == 0) { db->doctors[i].is_active = 0; success = 1; } }
        else if (type == 2) { for (int i = 0; i < total; i++) if (strcmp(db->employees[i].id, target_id) == 0) { db->employees[i].is_active = 0; success = 1; } }
        else if (type == 3) { for (int i = 0; i < total; i++) if (strcmp(db->lab_assts[i].id, target_id) == 0) { db->lab_assts[i].is_active = 0; success = 1; } }
        else if (type == 4) { for (int i = 0; i < total; i++) if (strcmp(db->patients[i].id, target_id) == 0) { db->patients[i].is_active = 0; success = 1; } }
        else if (type == 5) { for (int i = 0; i < total; i++) if (strcmp(db->admins[i].id, target_id) == 0) { db->admins[i].is_active = 0; success = 1; } }
        
        if (success) { saveDatabase(db); printf("[SUCCESS] User Account Deactivated.\n"); }
        else { printf("[ERROR] ID not found.\n"); }
        pauseSystem();
    } else if (action == 3 && type == 4) {
        char target_id[ID_LEN];
        printf("Enter Patient ID to view details: "); safeInput(target_id, ID_LEN);
        Patient *p = getPatientByID(db, target_id);
        
        if (p) {
            printf("\n--- BASIC PATIENT REGISTRATION DETAILS ---\n");
            printf("ID: %s\nName: %s\nEmail: %s\nAge: %d\nGender: %s\nBlood Group: %s\nPhone: %s\nAllergies: %s\n",
                   p->id, p->name, p->email, p->age, p->gender, p->blood_group, p->contact, p->allergies);
            
            // Re-using the built-in clinical history display to show bills, lab results, and prescriptions seamlessly
            displayPatientHistory(db, p);
        } else {
            printf("\n[ERROR] Patient ID not found or account is deactivated.\n");
            pauseSystem();
        }
    }
}

void adminUpdateUser(Database *db, const char* admin_id) {
    printHeader("ADMIN: UPDATE USER DETAILS");
    printf("Select Role to Edit: (1) Doctor, (2) Employee, (3) Lab Asst, (4) Patient, (5) Admin\nChoice: ");
    int role = getValidInt(1, 5);

    // --- NEW ADDITION: Display current users for the selected role ---
    printf("\n--- CURRENT ACTIVE USERS ---\n");
    int count = 0;
    char plainPass[MAX_STR];
    
    if (role == 1) {
        for (int i = 0; i < db->doc_count; i++) 
            if (db->doctors[i].is_active) { 
                decryptPasswordHex(db->doctors[i].password, plainPass);
                printf("  ID: %-10s | Name: Dr. %-15s | Qual: %-10s | Room: %-5s | Pass: %s\n", db->doctors[i].id, db->doctors[i].name, db->doctors[i].qualification, db->doctors[i].room_number, plainPass); 
                count++; 
            }
    } else if (role == 2) {
        for (int i = 0; i < db->emp_count; i++) 
            if (db->employees[i].is_active) { 
                decryptPasswordHex(db->employees[i].password, plainPass);
                printf("  ID: %-10s | Name: %-15s | Desig: %-15s | Shift: %-10s | Pass: %s\n", db->employees[i].id, db->employees[i].name, db->employees[i].designation, db->employees[i].shift, plainPass); 
                count++; 
            }
    } else if (role == 3) {
        for (int i = 0; i < db->lab_count; i++) 
            if (db->lab_assts[i].is_active) { 
                decryptPasswordHex(db->lab_assts[i].password, plainPass);
                printf("  ID: %-10s | Name: %-15s | Qual: %-10s | Spec: %-15s | Pass: %s\n", db->lab_assts[i].id, db->lab_assts[i].name, db->lab_assts[i].qualification, db->lab_assts[i].specialization, plainPass); 
                count++; 
            }
    } else if (role == 4) {
        for (int i = 0; i < db->pat_count; i++) {
            // REMOVED 'is_active' CHECK: Shows all patients universally
            decryptPasswordHex(db->patients[i].password, plainPass);
            printf("  ID: %-10s | Name: %-15s | Age: %d | Blood: %-3s | Allergies: %-15s | Pass: %s | Status: %s\n", db->patients[i].id, db->patients[i].name, db->patients[i].age, db->patients[i].blood_group, db->patients[i].allergies, plainPass, db->patients[i].is_active ? "Active" : "Deleted"); 
            count++; 
        }
    } else if (role == 5) {
        for (int i = 0; i < db->admin_count; i++) 
            if (db->admins[i].is_active) { 
                decryptPasswordHex(db->admins[i].password, plainPass);
                printf("  ID: %-10s | Name: %-15s | Pass: %s\n", db->admins[i].id, db->admins[i].name, plainPass); 
                count++; 
            }
    }
    
    if (count == 0) {
        printf("  [INFO] No active users found for this role.\n");
    }
    printf("----------------------------\n\n");
    // --- END OF NEW ADDITION ---

    char target_id[ID_LEN];
    printf("Enter User ID to Update: "); safeInput(target_id, ID_LEN);

    int found = 0;
    char *namePtr = NULL, *emailPtr = NULL, *phonePtr = NULL, *passPtr = NULL;

    if (role == 1) {
        for (int i = 0; i < db->doc_count; i++) 
            if (strcmp(db->doctors[i].id, target_id) == 0) { found=1; namePtr=db->doctors[i].name; emailPtr=db->doctors[i].email; phonePtr=db->doctors[i].contact; passPtr=db->doctors[i].password; break; }
    } else if (role == 2) {
        for (int i = 0; i < db->emp_count; i++) 
            if (strcmp(db->employees[i].id, target_id) == 0) { found=1; namePtr=db->employees[i].name; emailPtr=db->employees[i].email; passPtr=db->employees[i].password; break; }
    } else if (role == 3) {
        for (int i = 0; i < db->lab_count; i++) 
            if (strcmp(db->lab_assts[i].id, target_id) == 0) { found=1; namePtr=db->lab_assts[i].name; emailPtr=db->lab_assts[i].email; phonePtr=db->lab_assts[i].contact; passPtr=db->lab_assts[i].password; break; }
    } else if (role == 4) {
        for (int i = 0; i < db->pat_count; i++) 
            if (strcmp(db->patients[i].id, target_id) == 0) { found=1; namePtr=db->patients[i].name; emailPtr=db->patients[i].email; phonePtr=db->patients[i].contact; passPtr=db->patients[i].password; break; }
    } else if (role == 5) {
        for (int i = 0; i < db->admin_count; i++) 
            if (strcmp(db->admins[i].id, target_id) == 0) { found=1; namePtr=db->admins[i].name; emailPtr=db->admins[i].email; passPtr=db->admins[i].password; break; }
    }

    if (found) {
        printf("\nUser Found: %s\n", namePtr);
        printf("What would you like to update?\n(1) Name\n(2) Email\n(3) Phone Number\n(4) Password\n(5) Role-Specific Details\n(6) Exit/Cancel\nChoice: ");
        int ch = getValidInt(1, 6);
        
        if (ch == 6) {
            printf("[INFO] Update operation aborted.\n");
            pauseSystem(); return;
        }
        
        char tmp[MAX_STR];
        int is_updated = 0; // NEW: Track if actual modifications occur

        if (ch == 1) { 
            printf("Current Name: %s\n", namePtr); 
            printf("Enter New Name (Press Enter to keep current): "); safeInput(tmp, MAX_STR); 
            if (strlen(tmp) > 0 && tmp[0] != ' ' && strcmp(namePtr, tmp) != 0) { strcpy(namePtr, tmp); is_updated = 1; }
        }
        else if (ch == 2) { 
            printf("Current Email: %s\n", emailPtr); 
            printf("Enter New Email (Press Enter to keep current): "); safeInput(tmp, MAX_STR);
            if (strlen(tmp) > 0 && strcmp(emailPtr, tmp) != 0) {
                if (isEmailUnique(db, tmp)) { strcpy(emailPtr, tmp); is_updated = 1; }
                else { printf("[ERROR] Email already in use.\n"); pauseSystem(); return; }
            }
        }
        else if (ch == 3 && phonePtr != NULL) { 
            printf("Current Phone: %s\n", phonePtr); 
            printf("Enter New Phone (Press Enter to keep current): "); safeInput(tmp, 20); 
            if (strlen(tmp) > 0 && strcmp(phonePtr, tmp) != 0) { strcpy(phonePtr, tmp); is_updated = 1; }
        }
        else if (ch == 3 && phonePtr == NULL) { 
            printf("[ERROR] This role does not store a phone number.\n"); 
        }
        else if (ch == 4) {
            char plain[MAX_STR];
            decryptPasswordHex(passPtr, plain);
            printf("Current Password: %s\n", plain);
            printf("Enter New Password (Press Enter to keep current): "); safeInput(tmp, sizeof(tmp));
            if (strlen(tmp) > 0) {
                char newEnc[MAX_STR];
                encryptPasswordHex(tmp, newEnc);
                if (strcmp(passPtr, newEnc) != 0) { strcpy(passPtr, newEnc); is_updated = 1; }
            }
        }
        else if (ch == 5) {
            if (role == 1) { // Doctor
                for (int i=0; i<db->doc_count; i++) if (strcmp(db->doctors[i].id, target_id) == 0) {
                    printf("Update (1) Specialization or (2) Room Number? Choice: ");
                    if (getValidInt(1, 2) == 1) {
                        printf("Current Specialization: %s\n", db->doctors[i].specialization);
                        printf("Enter New Specialization (Press Enter to keep current): ");
                        safeInput(tmp, MAX_STR); 
                        if (strlen(tmp) > 0 && strcmp(db->doctors[i].specialization, tmp) != 0) { strcpy(db->doctors[i].specialization, tmp); is_updated = 1; }
                    } else {
                        printf("Current Room Number: %s\n", db->doctors[i].room_number);
                        printf("Enter New Room Number (Press Enter to keep current): ");
                        safeInput(tmp, 10); 
                        if (strlen(tmp) > 0 && strcmp(db->doctors[i].room_number, tmp) != 0) { strcpy(db->doctors[i].room_number, tmp); is_updated = 1; }
                    }
                    break;
                }
            } else if (role == 2) { // Employee
                for (int i=0; i<db->emp_count; i++) if (strcmp(db->employees[i].id, target_id) == 0) {
                    printf("Update (1) Shift or (2) Designation? Choice: ");
                    if (getValidInt(1,2) == 1) { 
                        printf("Current Shift: %s\n", db->employees[i].shift);
                        printf("Enter New Shift (Press Enter to keep current): "); 
                        safeInput(tmp, 50); 
                        if (strlen(tmp) > 0 && strcmp(db->employees[i].shift, tmp) != 0) { strcpy(db->employees[i].shift, tmp); is_updated = 1; }
                    } else { 
                        printf("Current Designation: %s\n", db->employees[i].designation);
                        printf("Enter New Designation (Press Enter to keep current): "); 
                        safeInput(tmp, 50); 
                        if (strlen(tmp) > 0 && strcmp(db->employees[i].designation, tmp) != 0) { strcpy(db->employees[i].designation, tmp); is_updated = 1; }
                    }
                    break;
                }
            } else if (role == 3) { // Lab Asst
                for (int i=0; i<db->lab_count; i++) if (strcmp(db->lab_assts[i].id, target_id) == 0) {
                    printf("Update (1) Qualification or (2) Specialization? Choice: ");
                    if (getValidInt(1,2) == 1) { 
                        printf("Current Qualification: %s\n", db->lab_assts[i].qualification);
                        printf("Enter New Qualification (Press Enter to keep current): ");
                        safeInput(tmp, MAX_STR); 
                        if (strlen(tmp) > 0 && strcmp(db->lab_assts[i].qualification, tmp) != 0) { strcpy(db->lab_assts[i].qualification, tmp); is_updated = 1; }
                    } else {
                        printf("Current Specialization: %s\n", db->lab_assts[i].specialization);
                        printf("Enter New Specialization (Press Enter to keep current): ");
                        safeInput(tmp, MAX_STR); 
                        if (strlen(tmp) > 0 && strcmp(db->lab_assts[i].specialization, tmp) != 0) { strcpy(db->lab_assts[i].specialization, tmp); is_updated = 1; }
                    }
                    break;
                }
            } else if (role == 4) { // Patient
                for (int i=0; i<db->pat_count; i++) if (strcmp(db->patients[i].id, target_id) == 0) {
                    printf("Current Allergies: %s\n", db->patients[i].allergies);
                    printf("Enter Updated Allergies (Press Enter to keep current): ");
                    safeInput(tmp, MAX_STR); 
                    if (strlen(tmp) > 0 && strcmp(db->patients[i].allergies, tmp) != 0) { strcpy(db->patients[i].allergies, tmp); is_updated = 1; }
                    break;
                }
            } else if (role == 5) { // Admin
                printf("[INFO] No role-specific details available for Admins.\n");
            }
        }
        
        // NEW: Check if any fields were actually updated before saving to disk
        if (is_updated) {
            saveDatabase(db);
            writeAuditLog(admin_id, "UPDATED_USER_PROFILE", target_id);
            printf("\n[SUCCESS] User details updated successfully.\n");
        } else {
            printf("\n[INFO] No changes were made.\n");
        }
    } else { printf("\n[ERROR] User ID not found.\n"); }
    pauseSystem();
}

// ==========================================
// 16. DASHBOARDS (The Routing Engine)
// ==========================================

// ==========================================
// 16. DASHBOARDS (The Routing Engine) - UPGRADED DOCTOR DASHBOARD
// ==========================================
void doctorDashboard(Database *db, Doctor *doc) {
    int choice;
    do {
        printHeader("DOCTOR CLINICAL DASHBOARD");
        printf("Welcome, Dr. %s\n\n", doc->name);
        printf("1. View My Patient Queue (REQ_DOC)\n");
        printf("2. Conduct Patient Visit (Auto-shows active queue)\n");
        printf("3. Update Old Prescription\n");
        printf("4. Search Old Patient & View History\n");
        printf("5. View My Profile Details\n");
        printf("6. Update My Profile Settings\n");
        printf("7. Logout\nChoice: ");
        choice = getValidInt(1, 7);

        if (choice == 1) {
            printf("\n--- WAITING ROOM (ACTIVE PATIENTS) ---\n");
            printf("  %-10s | %-12s | %-25s | %-15s | %-15s\n", "Visit ID", "Patient", "Status", "Disease", "Lab Report");
            printf("  ----------------------------------------------------------------------------------------\n");
            int found = 0;
            for (int i = 0; i < db->visit_count; i++) {
                // Determine if patient is actively residing in a ward assigned to this doctor
                int is_ward_admitted = (db->visits[i].status < COMPLETED && db->visits[i].is_discharged == 0 && strcmp(db->visits[i].ward_bed, "Outpatient") != 0 && strcmp(db->visits[i].ward_bed, "Discharged") != 0 && strlen(db->visits[i].ward_bed) > 0);
                
                if (strcmp(db->visits[i].doc_id, doc->id) == 0 && (db->visits[i].status == REQ_DOC || is_ward_admitted)) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if(p) {
                        char statusText[50] = "Assigned to Doc";
                        if (db->visits[i].direct_admission) snprintf(statusText, sizeof(statusText), "Admitted to %s", db->visits[i].ward_bed);
                        
                        char disease[50] = "NA";
                        if (strlen(db->visits[i].diagnosis) > 0) strncpy(disease, db->visits[i].diagnosis, 49);
                        
                        char labRep[50] = "NA";
                        if (strcmp(db->visits[i].tests_required, "None") != 0 && strlen(db->visits[i].test_results) > 0) {
                            strncpy(labRep, db->visits[i].test_results, 49);
                        }
                        
                        printf("  %-10s | %-12.12s | %-25.25s | %-15.15s | %-15.15s\n", 
                               db->visits[i].visit_id, p->name, statusText, disease, labRep);
                        found = 1;
                    }
                }
            }
            if (!found) printf("  No patients waiting in your queue.\n");
            pauseSystem();
        } 
        else if (choice == 2) {
            // REQUIREMENT 1: Display active queue directly inside the option loop
            printf("\n--- ACTIVE QUEUE AVAILABLE FOR EVALUATION ---\n");
            int queueCount = 0;
            for (int i = 0; i < db->visit_count; i++) {
                int is_ward_admitted = (db->visits[i].status < COMPLETED && db->visits[i].is_discharged == 0 && strcmp(db->visits[i].ward_bed, "Outpatient") != 0 && strcmp(db->visits[i].ward_bed, "Discharged") != 0 && strlen(db->visits[i].ward_bed) > 0);
                
                if (strcmp(db->visits[i].doc_id, doc->id) == 0 && (db->visits[i].status == REQ_DOC || is_ward_admitted)) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if (p) {
                        char context[50] = "Waiting in Queue";
                        if (is_ward_admitted) snprintf(context, sizeof(context), "Ward: %s", db->visits[i].ward_bed);
                        printf(" -> [Visit ID: %s] Patient Name: %s (%s)\n", db->visits[i].visit_id, p->name, context);
                        queueCount++;
                    }
                }
            }
            if (queueCount == 0) {
                printf("No active waiting patient IDs in your queue right now.\n");
                pauseSystem();
                continue;
            }

            char v_id[ID_LEN];
            printf("\nEnter Visit ID from Queue listed above: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && strcmp(db->visits[i].doc_id, doc->id) == 0) {
                    v = &db->visits[i];
                    break;
                }
            }
            
            int v_is_ward_admitted = (v && v->status < COMPLETED && v->is_discharged == 0 && strcmp(v->ward_bed, "Outpatient") != 0 && strcmp(v->ward_bed, "Discharged") != 0 && strlen(v->ward_bed) > 0);
            
            if (v && (v->status == REQ_DOC || v_is_ward_admitted)) {
                Patient *p = getPatientByID(db, v->patient_id);
                printf("\n--- CLINICAL EVALUATION ---\n");
                printf("Enter Vitals (Current: %s): ", strlen(v->vitals) > 0 ? v->vitals : "None"); safeInput(v->vitals, MAX_STR);
                printf("Enter Symptoms (Current: %s): ", strlen(v->symptoms) > 0 ? v->symptoms : "None"); safeInput(v->symptoms, 200);
                printf("Enter Diagnosis (Current: %s): ", strlen(v->diagnosis) > 0 ? v->diagnosis : "None"); safeInput(v->diagnosis, 200);
                
                // ALLERGY WARNING SYSTEM & MEDICINE APPENDING LOGIC
                char new_meds[200];
                while(1) {
                    printf("Enter Additional Medicines Prescribed (Current: %s)\n(Type new meds to append, or 'None'): ", strlen(v->medicines) > 0 ? v->medicines : "None"); 
                    safeInput(new_meds, 200);
                    if (custom_strcasestr(p->allergies, "none") == NULL && custom_strcasestr(new_meds, "none") == NULL && strlen(new_meds) > 0) {
                        printf("\n[CRITICAL WARNING] Patient Allergies: %s\n", p->allergies);
                        printf("Does this prescription conflict with allergies? (1 = Yes, 0 = No): ");
                        if (getValidInt(0, 1) == 1) {
                            printf("--- Please re-prescribe safer alternatives ---\n");
                            continue;
                        }
                    }
                    break;
                }
                
                // Securely append the new medicines to the existing list avoiding buffer overflow
                if (strlen(new_meds) > 0 && custom_strcasestr(new_meds, "none") == NULL) {
                    if (strlen(v->medicines) == 0 || custom_strcasestr(v->medicines, "none") != NULL) {
                        strcpy(v->medicines, new_meds); // Overwrite if currently empty or set to 'None'
                    } else {
                        if (strlen(v->medicines) + strlen(new_meds) + 5 < 200) {
                            strcat(v->medicines, ", ");
                            strcat(v->medicines, new_meds);
                        }
                    }
                }
                
                printf("Enter Doctor's Advice (Current: %s): ", strlen(v->doc_advice) > 0 ? v->doc_advice : "None"); safeInput(v->doc_advice, 200);

                // Check if the patient is already in a ward. Only ask to admit if they are an outpatient, discharged, or new.
                if (v_is_ward_admitted) {
                    v->ward_doc_visits += 1; // Auto-increment ward visits when doctor conducts rounds
                    printf("\n[INFO] Patient is currently in Ward/Bed: %s. (Ward assignment retained)\n", v->ward_bed);
                    printf("[INFO] Doctor Ward Visits counter automatically updated to: %d\n", v->ward_doc_visits);
                }
                else if (strcmp(v->ward_bed, "Outpatient") == 0 || strcmp(v->ward_bed, "Discharged") == 0 || strlen(v->ward_bed) == 0) {
                    printf("\nAdmit Patient to Ward/Bed? (1 = Yes, 0 = No): ");
                    if(getValidInt(0, 1) == 1) {
                        printf("Specify Ward/Bed instructions: "); safeInput(v->ward_bed, 20);
                    } else {
                        strcpy(v->ward_bed, "Outpatient");
                    }
                } else {
                    printf("\n[INFO] Patient is currently in Ward/Bed: %s. (Ward assignment retained)\n", v->ward_bed);
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
            printf("\n--- YOUR PREVIOUSLY EVALUATED PATIENTS ---\n");
            int count = 0;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].doc_id, doc->id) == 0 && db->visits[i].status >= REQ_DOC) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if (p) {
                        printf("  Visit ID: %-10s | Patient: %-15s | Date: %s\n", db->visits[i].visit_id, p->name, db->visits[i].date);
                        count++;
                    }
                }
            }
            if (count == 0) {
                printf("  [INFO] No evaluated prescriptions found to update.\n");
                pauseSystem();
                continue;
            }
            printf("------------------------------------------\n");

            char v_id[ID_LEN];
            printf("\nEnter Visit ID to Update Prescription: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && strcmp(db->visits[i].doc_id, doc->id) == 0) {
                    v = &db->visits[i];
                    break;
                }
            }
            
            if (v && v->status >= REQ_DOC) {
                char tmp[250];
                int is_updated = 0;
                printf("\n--- UPDATING PRESCRIPTION (Press Enter to keep current) ---\n");
                
                printf("Current Vitals: %s\nNew Vitals: ", v->vitals);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->vitals, tmp) != 0) { strcpy(v->vitals, tmp); is_updated = 1; }
                
                printf("Current Symptoms: %s\nNew Symptoms: ", v->symptoms);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->symptoms, tmp) != 0) { strcpy(v->symptoms, tmp); is_updated = 1; }
                
                printf("Current Diagnosis: %s\nNew Diagnosis: ", v->diagnosis);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->diagnosis, tmp) != 0) { strcpy(v->diagnosis, tmp); is_updated = 1; }
                
                printf("Current Medicines: %s\nNew Medicines: ", v->medicines);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->medicines, tmp) != 0) { strcpy(v->medicines, tmp); is_updated = 1; }
                
                printf("Current Advice: %s\nNew Advice: ", v->doc_advice);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->doc_advice, tmp) != 0) { strcpy(v->doc_advice, tmp); is_updated = 1; }
                
                if (is_updated) {
                    saveDatabase(db);
                    exportPrescription(db, v); 
                    writeAuditLog(doc->id, "UPDATED_PRESCRIPTION_FULL", v->visit_id);
                    printf("[SUCCESS] Prescription updated & document re-exported.\n");
                } else {
                    printf("[INFO] No changes were made to the prescription.\n");
                }
           } else { 
                printf("[ERROR] Visit ID not found or unauthorized.\n");
           }
            pauseSystem();
        }
        else if (choice == 4) {
            printHeader("SEARCH MY HISTORICAL PATIENTS");
            
            printf("--- ASSIGNED PATIENTS HISTORY DIRECTORY ---\n");
            int historyCount = 0;
            int *alreadyPrinted = (int *)calloc(db->pat_count, sizeof(int));
            
            for (int i = 0; i < db->visit_count; i++) {
                // Ensure the visit belongs to this doctor AND has actually been evaluated (status > REQ_DOC)
                if (strcmp(db->visits[i].doc_id, doc->id) == 0 && db->visits[i].status > REQ_DOC) {
                    for(int j = 0; j < db->pat_count; j++) {
                        if(strcmp(db->patients[j].id, db->visits[i].patient_id) == 0 && db->patients[j].is_active && !alreadyPrinted[j]) {
                            printf("  ID: %-10s | Name: %-15s | Phone: %-15s | Email: %s\n", 
                                   db->patients[j].id, db->patients[j].name, db->patients[j].contact, db->patients[j].email);
                            alreadyPrinted[j] = 1;
                            historyCount++;
                        }
                    }
                }
            }
            free(alreadyPrinted);
            
            if (historyCount == 0) {
                printf("  [INFO] You have not processed records for any patients yet.\n");
                printf("-------------------------------------------\n");
                pauseSystem();
                continue;
            }
            printf("-------------------------------------------\n\n");

            char query[MAX_STR];
            printf("Enter Patient Email or Phone Number from your list: "); safeInput(query, MAX_STR);
            
            Patient *found_pat = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].doc_id, doc->id) == 0 && db->visits[i].status > REQ_DOC) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if (p && (strcmp(p->email, query) == 0 || strcmp(p->contact, query) == 0)) {
                        found_pat = p;
                        break;
                    }
                }
            }

            if (found_pat) {
                printf("\n[FOUND] ID: %s | Name: %s | Age: %d | Blood: %s\n", 
                       found_pat->id, found_pat->name, found_pat->age, found_pat->blood_group);
                
                // CUSTOM HISTORY VIEWER: Only show visits associated with THIS specific doctor
                printf("\n========================================================\n");
                printf("        MEDICAL HISTORY (YOUR VISITS ONLY): %s\n", found_pat->name);
                printf("========================================================\n");
                int found_history = 0;
                for (int k = 0; k < db->visit_count; k++) {
                    if (strcmp(db->visits[k].patient_id, found_pat->id) == 0 && 
                        strcmp(db->visits[k].doc_id, doc->id) == 0 && 
                        db->visits[k].status > REQ_DOC) {
                        
                        printf("\n[Date: %s] | Visit ID: %s | Status: %s\n", db->visits[k].date, db->visits[k].visit_id, db->visits[k].is_paid ? "PAID" : "UNPAID");
                        printf("Attending Physician: Dr. %s (You)\n", doc->name);
                        printf("Vitals: %s\n", db->visits[k].vitals);
                        printf("Diagnosis: %s\n", db->visits[k].diagnosis);
                        printf("Prescription: %s\n", db->visits[k].medicines);
                        
                        if (strcmp(db->visits[k].tests_required, "None") != 0) {
                            printf("Lab Tests: %s | Results: %s\n", db->visits[k].tests_required, db->visits[k].test_results);
                        }
                        printf("--------------------------------------------------------\n");
                        found_history = 1;
                    }
                }
                if (!found_history) printf("No prior completed evaluations found for this patient with you.\n");
                pauseSystem();
                
            } else {
                printf("[ERROR] No patient matches that data inside your assigned history records.\n");
                pauseSystem();
            }
        }
        // REQUIREMENT 2: View My Profile Details implementation
        else if (choice == 5) {
            printHeader("MY PROFILE DETAILS");
            printf("Staff ID      : %s\n", doc->id);
            printf("Full Name     : Dr. %s\n", doc->name);
            printf("Email Address : %s\n", doc->email);
            printf("Qualifications: %s\n", doc->qualification);
            printf("Specialization: %s\n", doc->specialization);
            printf("Assigned Room : %s\n", doc->room_number);
            printf("Contact Phone : %s\n", doc->contact);
            printf("Account Status: %s\n", doc->is_active ? "Active" : "Deactivated");
            pauseSystem();
        }
        else if (choice == 6) { 
            printHeader("UPDATE MY PROFILE");
            printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Contact Phone\n(4) Specialization\n(5) Exit / Cancel\nChoice: ");
            int ch = getValidInt(1, 5);
            
            if (ch == 5) {
                printf("[INFO] Update operation aborted.\n");
            } else {
                char tmp[MAX_STR];
                int is_updated = 0;
                
                if (ch == 1) {
                    printf("Current Name: %s\n", doc->name);
                    printf("Enter New Name (Press Enter to keep current): "); safeInput(tmp, MAX_STR);
                    if (strlen(tmp) > 0 && tmp[0] != ' ' && strcmp(doc->name, tmp) != 0) { strcpy(doc->name, tmp); is_updated = 1; }
                } else if (ch == 2) {
                    char plainPass[MAX_STR];
                    decryptPasswordHex(doc->password, plainPass);
                    printf("Current Password: %s\n", plainPass);
                    
                    printf("Enter New Password (Press Enter to keep current): "); safeInput(tmp, sizeof(tmp));
                    if (strlen(tmp) > 0) {
                        char newEnc[MAX_STR];
                        encryptPasswordHex(tmp, newEnc);
                        if (strcmp(doc->password, newEnc) != 0) { strcpy(doc->password, newEnc); is_updated = 1; }
                    }
                } else if (ch == 3) {
                    printf("Current Phone: %s\n", doc->contact);
                    printf("Enter New Phone (Press Enter to keep current): "); safeInput(tmp, 20);
                    if (strlen(tmp) > 0 && strcmp(doc->contact, tmp) != 0) { strcpy(doc->contact, tmp); is_updated = 1; }
                } else if (ch == 4) {
                    printf("Current Specialization: %s\n", doc->specialization);
                    printf("Enter New Specialization (Press Enter to keep current): "); safeInput(tmp, MAX_STR);
                    if (strlen(tmp) > 0 && strcmp(doc->specialization, tmp) != 0) { strcpy(doc->specialization, tmp); is_updated = 1; }
                }
                
                if (is_updated) {
                    saveDatabase(db);
                    writeAuditLog(doc->id, "UPDATED_OWN_PROFILE", doc->id);
                    printf("[SUCCESS] Profile updated.\n");
                } else {
                    printf("[INFO] No changes were made.\n");
                }
            }
            pauseSystem();
        }
    } while (choice != 7); // Extended execution boundary safely matching choice 7 as logout
}
void employeeDashboard(Database *db, Employee *emp) {
    int choice;
    do {
        printHeader("RECEPTION & BILLING DASHBOARD");
        printf("Welcome, %s\n\n", emp->name);
        printf(" 1. Register New Walk-in Patient\n");
        printf(" 2. View Doctor Directory\n");
        printf(" 3. Create Visit and Assign\n");
        printf(" 4. Update Patient Demographics\n");
        printf(" 5. Search Old Patient & View History\n");
        printf(" 6. View All Active Patients (Directory)\n");
        printf(" 7. Process Billing & Checkout (REQ_BILL)\n");
        printf(" 8. Discharge Patient\n");
        printf(" 9. Assign Ward to a Patient\n");
        printf(" 10. Assign Lab Assistant to a Patient\n");
        printf(" 11. View My Profile\n");
        printf(" 12. Update My Profile\n");
        printf(" 13. View Available Patients (Ready for Visit)\n");
        printf(" 14. Pharmacy Billing (Walk-ins & Patients)\n");
        printf(" 15. View Pharmacy Sales Database\n");
        printf(" 16. Logout\nChoice: ");
        choice = getValidInt(1, 16);

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
            int keep_going = 1;
            while (keep_going) {
                VisitRecord v = {0};
                generateUniqueID("visits_db.txt", "VIS", v.visit_id);
                getCurrentTimestamp(v.date);
                v.is_paid = 0; v.total_bill = 0; v.doc_fee = 0; v.med_cost = 0; v.test_cost = 0;
                
                printf("\n--- CREATE VISIT AND ASSIGN ---\n");
                printf("Assign Patient to: (1) Doctor, (2) Lab Assistant, (3) Ward/Admission\nChoice: ");
                int assign_type = getValidInt(1, 3);

                printf("\n--- AVAILABLE PATIENTS (NO ONGOING VISITS) ---\n");
                int pCount = 0;
                for(int i=0; i<db->pat_count; i++) {
                    if(db->patients[i].is_active) {
                        int has_ongoing = 0;
                        for(int j=0; j<db->visit_count; j++) {
                            if(strcmp(db->visits[j].patient_id, db->patients[i].id) == 0 && db->visits[j].status < COMPLETED) {
                                has_ongoing = 1; break;
                            }
                        }
                        if (!has_ongoing) {
                            printf(" - ID: %-10s | Name: %-15s | Status: Ready for Visit\n", db->patients[i].id, db->patients[i].name);
                            pCount++;
                        }
                    }
                }
                if (pCount == 0) { 
                    printf(" No available patients found. All registered patients are currently in an ongoing visit or none exist.\n"); 
                } else {
                    printf("\n--- ASSIGN PATIENT ---\n");
                    printf("Enter Patient ID: "); safeInput(v.patient_id, ID_LEN);
                    if(!getPatientByID(db, v.patient_id)) { 
                        printf("[ERROR] Patient not found.\n"); 
                    } else {
                        if (assign_type == 1) { // 1. ASSIGN TO DOCTOR
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
                            if (assignedDoc) {
                                v.status = REQ_DOC;
                                strcpy(v.lab_id, "PENDING"); 
                                strcpy(v.ward_bed, "Outpatient");
                                strcpy(v.tests_required, "None");
                                addVisitToDB(db, v); saveDatabase(db);
                                writeAuditLog(emp->id, "CREATED_VISIT_DOC", v.visit_id);
                                printf("\n[SUCCESS] Visit created! Please direct patient to Dr. %s in Room %s.\n", assignedDoc->name, assignedDoc->room_number); 
                            }
                        } 
                        else if (assign_type == 2) { // 2. ASSIGN TO LAB ASSISTANT
                            printf("\nAvailable Lab Assistants:\n");
                            int l_count = 0;
                            for(int i=0; i<db->lab_count; i++) {
                                if(db->lab_assts[i].is_active) {
                                    printf(" - ID: %s | Name: %s (%s)\n", db->lab_assts[i].id, db->lab_assts[i].name, db->lab_assts[i].specialization);
                                    l_count++;
                                }
                            }
                            if (l_count == 0) {
                                printf("[ERROR] No Lab Assistants available.\n");
                            } else {
                                int l_found = 0;
                                while (!l_found) {
                                    printf("\nEnter Lab Assistant ID to assign (or type CANCEL): "); 
                                    safeInput(v.lab_id, ID_LEN);
                                    if (strcmp(v.lab_id, "CANCEL") == 0) break;
                                    
                                    for(int i=0; i<db->lab_count; i++) {
                                        if (strcmp(db->lab_assts[i].id, v.lab_id) == 0 && db->lab_assts[i].is_active) {
                                            l_found = 1; break;
                                        }
                                    }
                                    if (!l_found) printf("[ERROR] Invalid Lab Assistant ID. Try again.\n");
                                }
                                if (l_found) {
                                    printf("Enter required tests (e.g., Blood Test, X-Ray): ");
                                    safeInput(v.tests_required, 200);
                                    v.status = REQ_LAB;
                                    strcpy(v.doc_id, "PENDING");
                                    strcpy(v.ward_bed, "Outpatient");
                                    addVisitToDB(db, v); saveDatabase(db);
                                    writeAuditLog(emp->id, "CREATED_VISIT_LAB", v.visit_id);
                                    printf("\n[SUCCESS] Visit created! Patient assigned directly to Lab Assistant.\n"); 
                                }
                            }
                        } 
                        else if (assign_type == 3) { // 3. ASSIGN TO WARD (Requires Doctor)
                            printf("\nEnter Ward and Bed Number (e.g., General-12): ");
                            safeInput(v.ward_bed, 20);
                            
                            printf("\n[INFO] You must assign a Doctor to the admitted patient.\n");
                            printf("Available Doctors:\n");
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
                            if (assignedDoc) {
                                v.status = REQ_DOC; // Assign to doctor so they show up in their queue
                                strcpy(v.lab_id, "PENDING");
                                strcpy(v.tests_required, "None");
                                v.direct_admission = 1; // FLAG: Direct Admission (Waives first fee)
                                addVisitToDB(db, v); saveDatabase(db);
                                writeAuditLog(emp->id, "CREATED_VISIT_WARD", v.visit_id);
                                printf("\n[SUCCESS] Patient admitted to %s and assigned to Dr. %s.\n", v.ward_bed, assignedDoc->name); 
                            }
                        }
                    }
                }
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
        else if (choice == 4) {
            int keep_going = 1;
            while (keep_going) {
                printf("\n--- CURRENT ACTIVE PATIENTS ---\n");
                int pCount = 0;
                char plainPass[MAX_STR];
                for(int i = 0; i < db->pat_count; i++) {
                    if(db->patients[i].is_active) {
                        decryptPasswordHex(db->patients[i].password, plainPass);
                        char billStat[20] = "PAID/NONE";
                        for(int j=0; j<db->visit_count; j++) {
                            if(strcmp(db->visits[j].patient_id, db->patients[i].id) == 0) {
                                if(!db->visits[j].is_paid && db->visits[j].status >= REQ_DOC && db->visits[j].status < COMPLETED) {
                                    strcpy(billStat, "UNPAID");
                                    break;
                                }
                            }
                        }
                        printf("  ID: %-10s | Name: %-15s | Age: %d | Gender: %s | Blood: %s | Phone: %s | Pass: %s | Allergies: %s | Bill: %s\n", 
                            db->patients[i].id, db->patients[i].name, db->patients[i].age, db->patients[i].gender, db->patients[i].blood_group, db->patients[i].contact, plainPass, db->patients[i].allergies, billStat);
                        pCount++;
                    }
                }
                if (pCount == 0) {
                    printf("  [INFO] No active patients found. Please register one first.\n");
                } else {
                    char pId[ID_LEN];
                    printf("\nEnter Patient ID to Update: "); safeInput(pId, ID_LEN);
                    Patient *p = getPatientByID(db, pId);
                    if (!p) { printf("\n[ERROR] Patient not found or inactive.\n"); }
                    else {
                        printf("\n--- UPDATING: %s ---\n", p->name);
                        char tmp[MAX_STR];
                        int is_updated = 0; 
                        
                        printf("New Name (Enter to keep '%s'): ", p->name);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && strcmp(p->name, tmp) != 0) { strcpy(p->name, tmp); is_updated = 1; }
                        
                        printf("New Email (Enter to keep '%s'): ", p->email);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && strcmp(p->email, tmp) != 0) {
                            if (isEmailUnique(db, tmp)) { strcpy(p->email, tmp); is_updated = 1; }
                            else { printf("[WARNING] Email in use. Keeping old email.\n"); }
                        }
                        
                        printf("New Password (Enter to keep current hidden): ");
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0) { 
                            char newEnc[MAX_STR];
                            encryptPasswordHex(tmp, newEnc);
                            if (strcmp(p->password, newEnc) != 0) { strcpy(p->password, newEnc); is_updated = 1; }
                        }
                        
                        printf("New Age (Enter to keep '%d'): ", p->age);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && atoi(tmp) != p->age) { p->age = atoi(tmp); is_updated = 1; }
                        
                        printf("New Gender (Enter to keep '%s'): ", p->gender);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && strcmp(p->gender, tmp) != 0) { strcpy(p->gender, tmp); is_updated = 1; }
                        
                        printf("New Blood Group (Enter to keep '%s'): ", p->blood_group);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && strcmp(p->blood_group, tmp) != 0) { strcpy(p->blood_group, tmp); is_updated = 1; }
                        
                        printf("New Contact Phone (Enter to keep '%s'): ", p->contact);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && strcmp(p->contact, tmp) != 0) { strcpy(p->contact, tmp); is_updated = 1; }
                        
                        printf("New Allergies (Enter to keep '%s'): ", p->allergies);
                        safeInput(tmp, sizeof(tmp)); 
                        if (strlen(tmp) > 0 && strcmp(p->allergies, tmp) != 0) { strcpy(p->allergies, tmp); is_updated = 1; }
                        
                        if (is_updated) {
                            saveDatabase(db);
                            writeAuditLog(emp->id, "UPDATED_PATIENT", p->id);
                            printf("\n[SUCCESS] Patient comprehensive details updated.\n");
                        } else {
                            printf("\n[INFO] No details updated.\n");
                        }
                    }
                }
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
        else if (choice == 5) {
            int keep_going = 1;
            while (keep_going) {
                Patient *found_pat = searchPatient(db);
                if (found_pat) displayPatientHistory(db, found_pat);
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
      else if (choice == 6) {
            printf("\n--- CURRENT ACTIVE PATIENTS DIRECTORY ---\n");
            printf("  %-10s | %-12s | %-45s | %-15s | %-15s\n", "Patient ID", "Name", "Status", "Disease", "Lab Report");
            printf("  ---------------------------------------------------------------------------------------------------------------\n");
            int count = 0;
            for (int i = 0; i < db->pat_count; i++) {
                Patient *p = &db->patients[i];
                if (p->is_active) {
                    char statusText[100] = "NA";
                    char diseaseText[50] = "NA";
                    char labText[50] = "NA";
                    int is_active = 0;

                    for (int j = 0; j < db->visit_count; j++) {
                        if (strcmp(db->visits[j].patient_id, p->id) == 0 && db->visits[j].status < COMPLETED && !db->visits[j].is_paid) {
                            VisitRecord *v = &db->visits[j];
                            is_active = 1;
                            
                            Doctor *doc_ptr = getDoctorByID(db, v->doc_id);
                            char dName[50] = "Unknown";
                            char dRoom[20] = "Unknown";
                            if (doc_ptr) {
                                strcpy(dName, doc_ptr->name);
                                strcpy(dRoom, doc_ptr->room_number);
                            }

                            if (strlen(v->diagnosis) > 0) strncpy(diseaseText, v->diagnosis, 49);
                            if (strcmp(v->tests_required, "None") != 0) {
                                if (strlen(v->test_results) > 0) strncpy(labText, v->test_results, 49);
                                else strcpy(labText, "Pending");
                            }

                            if (v->is_discharged) {
                                snprintf(statusText, sizeof(statusText), "Discharged from %s", v->ward_bed);
                            } else if (strcmp(v->ward_bed, "Outpatient") != 0 && strcmp(v->ward_bed, "Discharged") != 0 && strlen(v->ward_bed) > 0) {
                                snprintf(statusText, sizeof(statusText), "Admitted to %s", v->ward_bed);
                            } else if (v->status == REQ_DOC) {
                                snprintf(statusText, sizeof(statusText), "Assigned to Dr. %s | Room No. : %s", dName, dRoom);
                            } else if (v->status == REQ_LAB) {
                                snprintf(statusText, sizeof(statusText), "Assigned to %s Lab", v->tests_required);
                            } else if (v->status == REQ_BILL) {
                                if (strcmp(v->tests_required, "None") != 0 && strlen(v->test_results) > 0) {
                                    snprintf(statusText, sizeof(statusText), "Visited the %s Lab", v->tests_required);
                                } else {
                                    snprintf(statusText, sizeof(statusText), "Visited Dr. %s | Room No. : %s", dName, dRoom);
                                }
                            }
                            break; // Stop looking after finding the active visit
                        }
                    }
                    
                    if (is_active) {
                        printf("  %-10s | %-12.12s | %-45.45s | %-15.15s | %-15.15s\n", 
                               p->id, p->name, statusText, diseaseText, labText);
                        count++;
                        if (count % 15 == 0) pauseSystem();
                    }
                }
            }
            if (count == 0) printf("  No active patients currently in routing.\n");
            else printf("\n  Total: %d active patient(s).\n", count);
            pauseSystem();
        }
        else if (choice == 7) {
            int keep_going = 1;
            while (keep_going) {
                printf("\n--- PENDING CHECKOUTS & QUEUE MANAGEMENT ---\n");
                int found = 0;
                for(int i=0; i<db->visit_count; i++) {
                    if((db->visits[i].status == REQ_BILL || db->visits[i].status == REQ_DOC || db->visits[i].status == REQ_LAB) && !db->visits[i].is_paid) {
                        Patient *p = getPatientByID(db, db->visits[i].patient_id);
                        if(p) { printf("Visit ID: %s | Status Enum: %d | Patient: %s\n", db->visits[i].visit_id, db->visits[i].status, p->name); found = 1; }
                    }
                }
                if(!found) { 
                    printf("Queue is empty.\n"); 
                    printf("\nPress enter to continue...\n");
                    int c; while ((c = getchar()) != '\n' && c != EOF); // Clear any leftover newline to genuinely pause
                    break; 
                }

                char v_id[ID_LEN];
                printf("\nEnter Visit ID to Process or Cancel: "); safeInput(v_id, ID_LEN);
                int processed = 0;
                for(int i=0; i<db->visit_count; i++) {
                    if(strcmp(db->visits[i].visit_id, v_id) == 0) {
                        processed = 1;
                        printf("\nAction: (1) Process Bill, (2) CANCEL BILL (Return to Menu), (3) Cancel/Abort Visit: ");
                        int action = getValidInt(1, 3);
                        
                        if(action == 2) {
                            printf("[SYSTEM] Billing process cancelled. Returning to menu without modifications.\n");
                            break;
                        } else if (action == 3) {
                            // Condition: Check if patient has already interacted with Doc, Lab, or Ward
                            int visited_doc = (strlen(db->visits[i].doc_advice) > 0 || strlen(db->visits[i].vitals) > 0);
                            int visited_lab = (strlen(db->visits[i].test_results) > 0);
                            int in_ward = (strcmp(db->visits[i].ward_bed, "Outpatient") != 0 && strcmp(db->visits[i].ward_bed, "Discharged") != 0 && strlen(db->visits[i].ward_bed) > 0);

                            if (visited_doc || visited_lab || in_ward) {
                                printf("\n[ERROR] Cannot abort visit. Patient has already ");
                                if (visited_doc && visited_lab) printf("visited the doctor and lab.\n");
                                else if (visited_doc) printf("visited the doctor.\n");
                                else if (visited_lab) printf("visited the lab.\n");
                                else if (in_ward) printf("been admitted to the ward.\n");
                                printf("They must proceed to billing checkout.\n");
                            } else {
                                db->visits[i].status = CANCELLED_BY_PATIENT;
                                saveDatabase(db);
                                writeAuditLog(emp->id, "CANCELLED_VISIT", v_id);
                                printf("\n[SUCCESS] Visit permanently cancelled/aborted.\n");
                            }
                            break;
                        }

                        if (db->visits[i].status != REQ_BILL) {
                            printf("[ERROR] Patient has not finished clinical routing yet.\n"); break;
                        }

                        // REQUIREMENT 10: Block billing if admitted but not discharged
                        if (strcmp(db->visits[i].ward_bed, "Outpatient") != 0 && db->visits[i].is_discharged == 0 && strcmp(db->visits[i].ward_bed, "Discharged") != 0) {
                            printf("[ERROR] Patient is currently admitted in '%s'. You must discharge the patient first before proceeding to billing.\n", db->visits[i].ward_bed);
                            break;
                        }

                        printf("\n--- PRE-BILLING CLINICAL SUMMARY ---\n");
                        
                        int has_doc = (strcmp(db->visits[i].doc_id, "PENDING") != 0);

                        if (has_doc) {
                            printf("Doctor's Advice: %s\n", db->visits[i].doc_advice);
                        }
                        
                        // Show all appended lab tests cleanly, hide results line to prevent visual clutter
                        if (strcmp(db->visits[i].tests_required, "None") != 0 && strlen(db->visits[i].tests_required) > 0) {
                            printf("Lab Tests Conducted: %s\n", db->visits[i].tests_required);
                        }
                        printf("------------------------------------\n");

                        if (has_doc) {
                            printf("Doctor's Prescribed Medicines: %s\n", db->visits[i].medicines);
                            printf("Enter the final Pharmacy items purchased (Leave blank to keep prescription): ");
                        } else {
                            printf("Enter Pharmacy items purchased (Type 'None' or leave blank to skip): ");
                        }
                        
                        char extra_meds[200];
                        safeInput(extra_meds, 200);
                        if (strlen(extra_meds) > 0 && custom_strcasestr(extra_meds, "none") == NULL) {
                            strcpy(db->visits[i].medicines, extra_meds); // Overwrites with final actual list
                            printf("Updated Final Medicines List: %s\n", db->visits[i].medicines);
                        }

                       // NEW WARD & EMERGENCY BILLING LOGIC
                        double base_doc_fee = 0.0;
                        double initial_consult = 0.0;
                        double ward_doc_fee = 0.0;
                        char buf[50];
                        
                        // Completely hide consultation fee prompts if they never saw a doctor
                        if (has_doc) {
                            printf("\nEnter Doctor Base Consultation Fee Rate ($): ");
                            safeInput(buf, 50);
                            base_doc_fee = atof(buf);
                            
                            if (!db->visits[i].direct_admission) {
                                initial_consult = base_doc_fee;
                                printf(" -> Initial Consultation Fee (Non-Direct): $%.2f\n", initial_consult);
                            } else {
                                printf(" -> [Direct Ward Admission] Initial Consultation Fee Waived.\n");
                            }
                            
                            ward_doc_fee = db->visits[i].ward_doc_visits * base_doc_fee;
                            if (db->visits[i].ward_doc_visits > 0) {
                                printf(" -> Ward Visits Fee (%d visits @ $%.2f): $%.2f\n", db->visits[i].ward_doc_visits, base_doc_fee, ward_doc_fee);
                            }
                        }

                        // Only ask for pharmacy cost if they actually bought medicines
                        if (has_doc || (strlen(db->visits[i].medicines) > 0 && custom_strcasestr(db->visits[i].medicines, "none") == NULL)) {
                            printf("\nEnter Total Pharmacy Cost ($): "); 
                            safeInput(buf, 50); db->visits[i].med_cost = atof(buf);
                        } else {
                            db->visits[i].med_cost = 0.0;
                        }
                        
                        if (strcmp(db->visits[i].tests_required, "None") != 0 && strcmp(db->visits[i].lab_id, "PENDING") != 0) {
                            printf("Enter Lab Cost ($): "); 
                            safeInput(buf, 50); db->visits[i].test_cost = atof(buf);
                        } else { db->visits[i].test_cost = 0.0; }

                        double ward_cost = 0.0;
                        if (db->visits[i].is_discharged == 1 || strcmp(db->visits[i].ward_bed, "Discharged") == 0) {
                            int days = (db->visits[i].ward_days > 0) ? db->visits[i].ward_days : calculateDaysStayed(db->visits[i].date);
                            char w_name[30];
                            if (strcmp(db->visits[i].ward_bed, "Discharged") == 0) strcpy(w_name, "General/Unknown");
                            else strcpy(w_name, db->visits[i].ward_bed);

                            printf("\nEnter Daily Ward/Bed Charge for '%s' ($): ", w_name);
                            safeInput(buf, 50); 
                            double daily_rate = atof(buf);
                            ward_cost = daily_rate * days;
                            printf(" -> Total Ward Stay Cost (%d days @ $%.2f): $%.2f\n", days, daily_rate, ward_cost);
                        }
                        
                        // Aggregate Ward & Doctor costs into doc_fee for final bill total
                        db->visits[i].doc_fee = initial_consult + ward_doc_fee + ward_cost;
                        double expected_total = (db->visits[i].doc_fee + db->visits[i].med_cost + db->visits[i].test_cost) * 1.05;
                        
                        printf("\nTotal Bill amount is $%.2f. Collect Payment from Patient now? (1 = Yes, 0 = No): ", expected_total);
                        if(getValidInt(0, 1) == 1) {
                            db->visits[i].is_paid = 1;
                            db->visits[i].status = COMPLETED; 
                            
                            // AUTO-LOGGING TO PHARMACY DATABASE FOR CLINIC PATIENTS
                            if (db->visits[i].med_cost > 0 && strlen(db->visits[i].medicines) > 0 && custom_strcasestr(db->visits[i].medicines, "none") == NULL) {
                                Patient *p = getPatientByID(db, db->visits[i].patient_id);
                                if (p) {
                                    PharmacySale ps = {0};
                                    generateUniqueID("pharmacy_db.txt", "PHM", ps.sale_id);
                                    getCurrentTimestamp(ps.date);
                                    strcpy(ps.patient_id, p->id);
                                    strcpy(ps.name, p->name);
                                    strcpy(ps.contact, p->contact);
                                    strcpy(ps.email, p->email);
                                    strcpy(ps.medicines, db->visits[i].medicines);
                                    ps.cost = db->visits[i].med_cost;
                                    addPharmacySaleToDB(db, ps);
                                }
                            }
                            
                            saveDatabase(db);
                            writeAuditLog(emp->id, "PROCESSED_PAYMENT", v_id);
                            printf("[SUCCESS] Payment Collected & Pharmacy DB Updated.\n");
                        } else {
                            printf("[WARNING] Payment not collected. Invoice will be marked as UNPAID.\n");
                        }

                        exportInvoice(db, &db->visits[i]);
                        exportPrescription(db, &db->visits[i]); // NEW: Re-export prescription to append the fee
                        
                        if(db->visits[i].is_paid == 1) {
                            printf("[SYSTEM] Workflow Complete.\n");
                        }
                        break;
                    } // <-- CLOSES THE: if(strcmp(db->visits[i].visit_id, v_id) == 0)
                } // <-- MOVED BRACE: CLOSES THE FOR LOOP HERE!

                // These lines now safely execute only ONCE after the search loop finishes
                if (!processed) printf("[ERROR] Invalid Visit ID.\n");
                
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            } // <-- CLOSES THE: while (keep_going) loop
        } // <-- CLOSES THE: else if (choice == 7) block
        else if (choice == 8) {
            int keep_going = 1;
            while (keep_going) {
                printf("\n--- ADMITTED PATIENTS (WARD/BED) ---\n");
                int found = 0;
                for (int i = 0; i < db->visit_count; i++) {
                    if (db->visits[i].status < COMPLETED && strcmp(db->visits[i].ward_bed, "Outpatient") != 0 && strcmp(db->visits[i].ward_bed, "Discharged") != 0 && db->visits[i].is_discharged == 0) {
                        Patient *p = getPatientByID(db, db->visits[i].patient_id);
                        Doctor *d = getDoctorByID(db, db->visits[i].doc_id);
                        if (p && d) {
                            printf("Visit ID: %s | Patient: %s | Ward: %s | Dr. %s | Disease: %s\n", 
                                db->visits[i].visit_id, p->name, db->visits[i].ward_bed, d->name, 
                                strlen(db->visits[i].diagnosis) > 0 ? db->visits[i].diagnosis : "Pending Diagnosis");
                            found++;
                        }
                    }
                }
                
                if (found == 0) {
                    printf("No patients are currently admitted to a ward.\n");
                } else {
                    char v_id[ID_LEN];
                    printf("\nEnter Visit ID to Discharge Patient: "); safeInput(v_id, ID_LEN);
                    int discharged = 0;
                    for (int i = 0; i < db->visit_count; i++) {
                        if (strcmp(db->visits[i].visit_id, v_id) == 0 && db->visits[i].status < COMPLETED) {
                            
                            printf("Enter the Ward/Bed the patient stayed in (e.g., General-12): ");
                            safeInput(db->visits[i].ward_bed, 20);
                            printf("Enter exact number of days the patient stayed: ");
                            db->visits[i].ward_days = getValidInt(1, 1000);
                            printf("Enter number of times the Doctor visited the patient in the ward: ");
                            db->visits[i].ward_doc_visits = getValidInt(0, 1000);
                            db->visits[i].is_discharged = 1; // Mark as discharged cleanly
                            
                            saveDatabase(db);
                            writeAuditLog(emp->id, "DISCHARGED_FROM_WARD", v_id);
                            printf("[SUCCESS] Patient discharged from %s after staying %d day(s). Please route to billing.\n", db->visits[i].ward_bed, db->visits[i].ward_days);
                            discharged = 1; break;
                        }
                    }
                    if (!discharged) printf("[ERROR] Invalid Visit ID.\n");
                }
                
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
        else if (choice == 9) {
            int keep_going = 1;
            while(keep_going) {
                printf("\n--- ASSIGN WARD TO PATIENT ---\n");
                int found = 0;
                for (int i = 0; i < db->visit_count; i++) {
                    if (db->visits[i].status < COMPLETED && strcmp(db->visits[i].ward_bed, "Discharged") != 0) {
                        Patient *p = getPatientByID(db, db->visits[i].patient_id);
                        Doctor *d = getDoctorByID(db, db->visits[i].doc_id); // NEW: Get Doc Name
                        
                        if (p) {
                            char probStat[200];
                            if (strlen(db->visits[i].symptoms) > 0) strcpy(probStat, db->visits[i].symptoms);
                            else strcpy(probStat, "NA"); // Replaced "Not Evaluated Yet"

                            char advisedWard[30];
                            if (strlen(db->visits[i].ward_bed) == 0 || strcmp(db->visits[i].ward_bed, "Outpatient") == 0) strcpy(advisedWard, "NA");
                            else strcpy(advisedWard, db->visits[i].ward_bed);

                            char docName[50] = "NA";
                            if (d) snprintf(docName, sizeof(docName), "Dr. %s", d->name);

                            printf("Visit ID: %s | Name: %-15s | Doc: %-15s | Advised Ward: %-10s | Problems: %s\n", 
                                db->visits[i].visit_id, p->name, docName, advisedWard, probStat);
                            found = 1;
                        }
                    }
                }
                
                if (!found) {
                    printf("No active visits requiring ward assignment.\n");
                } else {
                    char v_id[ID_LEN];
                    printf("\nEnter Visit ID to Assign Ward: "); safeInput(v_id, ID_LEN);
                    int updated = 0;
                    for (int i = 0; i < db->visit_count; i++) {
                        if (strcmp(db->visits[i].visit_id, v_id) == 0 && db->visits[i].status < COMPLETED) {
                            printf("Enter New Ward/Bed for Patient (e.g., General-12, ICU-4): ");
                            safeInput(db->visits[i].ward_bed, 20);
                            saveDatabase(db);
                            writeAuditLog(emp->id, "ASSIGNED_WARD", v_id);
                            printf("[SUCCESS] Ward assigned.\n");
                            updated = 1;
                            break;
                        }
                    }
                    if (!updated) printf("[ERROR] Invalid Visit ID.\n");
                }
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
        else if (choice == 10) {
            int keep_going = 1;
            while(keep_going) {
                printf("\n--- ASSIGN LAB ASSISTANT TO PATIENT ---\n");
                int found = 0;
                for (int i = 0; i < db->visit_count; i++) {
                    // Changed condition: allow ANY active uncompleted visit to be assigned multiple lab tests
                    if (!db->visits[i].is_paid && db->visits[i].status < COMPLETED) {
                        Patient *p = getPatientByID(db, db->visits[i].patient_id);
                        if (p) {
                            printf("Visit ID: %s | Patient: %-15s | Current Tests: %s\n", 
                                db->visits[i].visit_id, p->name, db->visits[i].tests_required);
                            found = 1;
                        }
                    }
                }
                
                if (!found) {
                    printf("No active visits available for Lab Assistant assignment.\n");
                } else {
                    char v_id[ID_LEN];
                    printf("\nEnter Visit ID to Assign Lab Assistant: "); safeInput(v_id, ID_LEN);
                    int updated = 0;
                    for (int i = 0; i < db->visit_count; i++) {
                        if (strcmp(db->visits[i].visit_id, v_id) == 0 && !db->visits[i].is_paid && db->visits[i].status < COMPLETED) {
                            printf("\n--- AVAILABLE LAB ASSISTANTS ---\n");
                            int l_count = 0;
                            for (int j = 0; j < db->lab_count; j++) {
                                if (db->lab_assts[j].is_active) {
                                    printf(" - ID: %-10s | Name: %-15s | Specialization: %s\n", 
                                        db->lab_assts[j].id, db->lab_assts[j].name, db->lab_assts[j].specialization);
                                    l_count++;
                                }
                            }
                            if (l_count == 0) {
                                printf("[ERROR] No active Lab Assistants available in the system.\n");
                            } else {
                                char l_id[ID_LEN];
                                printf("\nEnter Lab Assistant ID to assign: "); safeInput(l_id, ID_LEN);
                                
                                int l_found = 0;
                                for (int j = 0; j < db->lab_count; j++) {
                                    if (strcmp(db->lab_assts[j].id, l_id) == 0 && db->lab_assts[j].is_active) {
                                        l_found = 1; break;
                                    }
                                }
                                if (l_found) {
                                    printf("\nCurrent Tests Requested: %s\n", db->visits[i].tests_required);
                                    printf("Enter additional tests to assign (Press Enter to just keep current tests): ");
                                    char new_test[100];
                                    safeInput(new_test, 100);
                                    
                                    // Only update if the employee actually typed something new to prevent duplicates
                                    if (strlen(new_test) > 0) {
                                        if (strcmp(db->visits[i].tests_required, "None") == 0 || strlen(db->visits[i].tests_required) == 0) {
                                            strcpy(db->visits[i].tests_required, new_test);
                                        } else {
                                            if (strlen(db->visits[i].tests_required) + strlen(new_test) + 5 < 200) {
                                                strcat(db->visits[i].tests_required, ", ");
                                                strcat(db->visits[i].tests_required, new_test);
                                            }
                                        }
                                    }
                                    
                                    db->visits[i].status = REQ_LAB; // Ensure it routes back to lab queue
                                    strcpy(db->visits[i].lab_id, l_id);
                                    saveDatabase(db);
                                    writeAuditLog(emp->id, "ASSIGNED_LAB_ASST", v_id);
                                    printf("\n[SUCCESS] Patient assigned to Lab Assistant %s. Final Tests: %s.\n", l_id, db->visits[i].tests_required);
                                } else {
                                    printf("[ERROR] Invalid Lab Assistant ID.\n");
                                }
                            }
                            updated = 1;
                            break;
                        }
                    }
                    if (!updated) printf("[ERROR] Invalid Visit ID.\n");
                }
                printf("\nDo you want to continue to do more in this option? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
        else if (choice == 11) {
            printHeader("MY PROFILE DETAILS");
            printf("Staff ID      : %s\n", emp->id);
            printf("Full Name     : %s\n", emp->name);
            printf("Email Address : %s\n", emp->email);
            printf("Designation   : %s\n", emp->designation);
            printf("Shift         : %s\n", emp->shift);
            printf("Account Status: %s\n", emp->is_active ? "Active" : "Deactivated");
            pauseSystem();
        }
        else if (choice == 12) { 
            printHeader("UPDATE MY PROFILE");
            printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Shift\n(4) Exit / Cancel\nChoice: ");
            int ch = getValidInt(1, 4);
            
            if (ch == 4) {
                printf("[INFO] Update operation aborted.\n");
            } else {
                char tmp[MAX_STR];
                int is_updated = 0; 

                if (ch == 1) {
                    printf("Current Name: %s\n", emp->name);
                    printf("Enter New Name: "); safeInput(tmp, sizeof(tmp));
                    if (strlen(tmp) > 0 && strcmp(emp->name, tmp) != 0) { strcpy(emp->name, tmp); is_updated = 1; }
                } else if (ch == 2) {
                    char plainPass[MAX_STR];
                    decryptPasswordHex(emp->password, plainPass);
                    printf("Current Password: %s\n", plainPass);
                    
                    printf("Enter New Password: "); safeInput(tmp, sizeof(tmp));
                    if (strlen(tmp) > 0) {
                        char newEnc[MAX_STR];
                        encryptPasswordHex(tmp, newEnc);
                        if (strcmp(emp->password, newEnc) != 0) { strcpy(emp->password, newEnc); is_updated = 1; }
                    }
                } else if (ch == 3) {
                    printf("Current Shift: %s\n", emp->shift);
                    printf("Enter New Shift: "); safeInput(tmp, 50);
                    if (strlen(tmp) > 0 && strcmp(emp->shift, tmp) != 0) { strcpy(emp->shift, tmp); is_updated = 1; }
                }
                
                if (is_updated) {
                    saveDatabase(db);
                    writeAuditLog(emp->id, "UPDATED_OWN_PROFILE", emp->id);
                    printf("[SUCCESS] Profile updated successfully.\n");
                } else {
                    printf("[INFO] No changes were made.\n");
                }
            }
            pauseSystem();
        }
        else if (choice == 13) {
            printf("\n--- AVAILABLE PATIENTS (NO ONGOING VISITS) ---\n");
            printf("  %-10s | %-20s | %-15s | %-25s\n", "Patient ID", "Name", "Phone", "Email");
            printf("  --------------------------------------------------------------------------------\n");
            int pCount = 0;
            for(int i = 0; i < db->pat_count; i++) {
                if(db->patients[i].is_active) {
                    int has_ongoing = 0;
                    // Check if patient is in any active visit state
                    for(int j = 0; j < db->visit_count; j++) {
                        if(strcmp(db->visits[j].patient_id, db->patients[i].id) == 0 && db->visits[j].status < COMPLETED) {
                            has_ongoing = 1; break;
                        }
                    }
                    if (!has_ongoing) {
                        printf("  %-10s | %-20.20s | %-15.15s | %-25.25s\n", 
                               db->patients[i].id, db->patients[i].name, db->patients[i].contact, db->patients[i].email);
                        pCount++;
                        if (pCount % 15 == 0) pauseSystem();
                    }
                }
            }
            if (pCount == 0) {
                printf("  [INFO] No available patients found. All active patients are currently in an ongoing visit.\n");
            } else {
                printf("\n  Total: %d available patient(s).\n", pCount);
            }
            pauseSystem();
        }
        else if (choice == 14) {
            int keep_going = 1;
            while(keep_going) {
                printf("\n--- STANDALONE PHARMACY BILLING ---\n");
                PharmacySale ps = {0};
                generateUniqueID("pharmacy_db.txt", "PHM", ps.sale_id);
                getCurrentTimestamp(ps.date);

                printf("Is this for a Walk-in or a Registered Patient?\n(1) Walk-in Customer, (2) Registered Patient\nChoice: ");
                int pt_type = getValidInt(1, 2);

                if (pt_type == 2) {
                    printf("\n--- REGISTERED PATIENT LIST ---\n");
                    int pCount = 0;
                    for (int i = 0; i < db->pat_count; i++) {
                        if (db->patients[i].is_active) {
                            printf("  ID: %-10s | Name: %-15s | Phone: %-15s | Status: Active\n", 
                                   db->patients[i].id, db->patients[i].name, db->patients[i].contact);
                            pCount++;
                        }
                    }
                    if (pCount == 0) {
                        printf("  [INFO] No registered patients found.\n");
                    }
                    printf("-------------------------------\n");

                    printf("Enter Patient ID: "); safeInput(ps.patient_id, ID_LEN);
                    Patient *p = getPatientByID(db, ps.patient_id);
                    if (p) {
                        strcpy(ps.name, p->name);
                        strcpy(ps.contact, p->contact);
                        strcpy(ps.email, p->email);
                        printf("[INFO] Linked to Patient Profile: %s\n", ps.name);
                    } else {
                        printf("[ERROR] Patient ID not found. Reverting to Walk-in.\n");
                        strcpy(ps.patient_id, "WALK-IN");
                        strcpy(ps.email, "NA");
                        printf("Enter Customer Name: "); safeInput(ps.name, MAX_STR);
                        printf("Enter Phone Number: "); safeInput(ps.contact, 20);
                    }
                } else {
                    strcpy(ps.patient_id, "WALK-IN");
                    strcpy(ps.email, "NA");
                    printf("Enter Customer Name: "); safeInput(ps.name, MAX_STR);
                    printf("Enter Phone Number: "); safeInput(ps.contact, 20);
                }

                printf("Enter the Medicines / Pharmacy items: "); safeInput(ps.medicines, 200);
                printf("Enter Total Cost of items ($): "); 
                char buf[50]; safeInput(buf, 50); ps.cost = atof(buf);

                addPharmacySaleToDB(db, ps);
                saveDatabase(db);
                writeAuditLog(emp->id, "MANUAL_PHARMACY_SALE", ps.sale_id);
                printf("\n[SUCCESS] Pharmacy sale recorded! ID: %s | Total: $%.2f\n", ps.sale_id, ps.cost);

                printf("\nProcess another pharmacy bill? (1 = Yes, 0 = No): ");
                keep_going = getValidInt(0, 1);
            }
        }
        else if (choice == 15) {
            viewPharmacyDatabase(db);
        }
    } while (choice != 16); 
}

// ==========================================
// 18. THE LAB REPORT EXPORT ENGINE
// ==========================================
void exportLabReport(Database *db, VisitRecord *v) {
    Patient *p = getPatientByID(db, v->patient_id);
    LabAssistant *l = NULL;
    for (int i = 0; i < db->lab_count; i++) {
        if (strcmp(db->lab_assts[i].id, v->lab_id) == 0 && db->lab_assts[i].is_active) {
            l = &db->lab_assts[i];
            break;
        }
    }
    if (!p || !l) return;

    char filename[MAX_STR];
    formatFilename(filename, p->name, "_LabReport.txt");
    
    FILE *f = fopen("temp_lab.tmp", "w");
    if (f) {
        fprintf(f, "*************************************************\n");
        fprintf(f, "               NEW LAB REPORT ENTRY              \n");
        char timeStr[50]; getCurrentTimestamp(timeStr);
        fprintf(f, "=================================================\n");
        fprintf(f, "         %s \n", CLINIC_NAME);
        fprintf(f, "         %s \n", CLINIC_ADDRESS);
        fprintf(f, "=================================================\n");
        fprintf(f, "Date: %s | Visit ID: %s\n", timeStr, v->visit_id);
        fprintf(f, "Patient: %-20s Phone: %s\n", p->name, p->contact);
        fprintf(f, "Age: %-5d Gender: %-10s Blood: %s\n", p->age, p->gender, p->blood_group);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "TESTS REQUESTED (By Doctor):\n%s\n", v->tests_required);
        fprintf(f, "-------------------------------------------------\n");
        fprintf(f, "OFFICIAL TEST RESULTS:\n%s\n", v->test_results);
        fprintf(f, "=================================================\n");
        fprintf(f, "Processed By: %s (ID: %s)\n", l->name, l->id);
        fprintf(f, "Qualifications: %s\n", l->qualification);
        fprintf(f, "Specialization: %s\n", l->specialization);
        fprintf(f, "=================================================\n\n");

        FILE *oldF = fopen(filename, "r");
        if (oldF) {
            char ch;
            while ((ch = fgetc(oldF)) != EOF) {
                fputc(ch, f);
            }
            fclose(oldF);
        }
        fclose(f);
        
        remove(filename);
        rename("temp_lab.tmp", filename);
        printf("\n[SYSTEM] Exported Lab Report to: %s (Newest on top)\n", filename);
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
        printf("3. Update Lab Report\n");
        printf("4. Search Old Patient & View History\n");
        printf("5. View My Profile\n");
        printf("6. Update My Profile\n");
        printf("7. Logout\nChoice: ");
        choice = getValidInt(1, 7);

        if (choice == 1) {
            printf("\n--- PENDING LAB QUEUE ---\n");
            int found = 0;
            for (int i = 0; i < db->visit_count; i++) {
                if (db->visits[i].status == REQ_LAB && strcmp(db->visits[i].lab_id, lab->id) == 0) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if(p) {
                        printf("Visit ID: %s | Patient: %s | Tests: %s\n", db->visits[i].visit_id, p->name, db->visits[i].tests_required);
                        found = 1;
                    }
                }
            }
            if (!found) printf("No pending lab work assigned to you.\n");
            pauseSystem();
        } 
        else if (choice == 2) {
            // --- NEW ADDITION: Show ONLY patients currently waiting for this specific step ---
            printf("\n--- YOUR PENDING ASSIGNED PATIENTS ---\n");
            int assignedCount = 0;
            for (int i = 0; i < db->visit_count; i++) {
                if (db->visits[i].status == REQ_LAB && strcmp(db->visits[i].lab_id, lab->id) == 0) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if (p) {
                        printf("  Visit ID: %-10s | Patient ID: %-10s | Name: %-15s | Tests: %s\n", 
                               db->visits[i].visit_id, p->id, p->name, db->visits[i].tests_required);
                        assignedCount++;
                    }
                }
            }
            if (assignedCount == 0) {
                printf("  [INFO] No pending clinical lab assignments found for your queue.\n");
                printf("----------------------------------------\n");
                pauseSystem();
                continue;
            }
            printf("----------------------------------------\n");

            char v_id[ID_LEN];
            printf("\nEnter Visit ID to Process: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && db->visits[i].status == REQ_LAB && strcmp(db->visits[i].lab_id, lab->id) == 0) {
                    v = &db->visits[i];
                    break;
                }
            }
            
            if (v) {
                printf("\n--- ENTER LAB RESULTS ---\n");
                printf("Requested Tests: %s\n", v->tests_required);
                
                char new_results[200];
                printf("Enter Official Results: "); safeInput(new_results, 200);
                
                // Securely append results to prevent data loss across multiple lab tests
                if (strlen(v->test_results) == 0 || custom_strcasestr(v->test_results, "None") != NULL || strcmp(v->test_results, "Pending") == 0) {
                    strcpy(v->test_results, new_results);
                } else {
                    if (strlen(v->test_results) + strlen(new_results) + 5 < 200) {
                        strcat(v->test_results, " | ");
                        strcat(v->test_results, new_results);
                    }
                }
                
                strcpy(v->lab_id, lab->id); // Log which assistant did the test
                v->status = REQ_BILL;       // Route back to Employee for billing
                
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
            printf("\n--- YOUR PREVIOUSLY PROCESSED LAB REPORTS ---\n");
            int count = 0;
            for (int i = 0; i < db->visit_count; i++) {
                // Ensure the visit was processed by THIS assistant and results were already given (status >= REQ_BILL)
                if (strcmp(db->visits[i].lab_id, lab->id) == 0 && db->visits[i].status >= REQ_BILL) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if (p) {
                        printf("  Visit ID: %-10s | Patient: %-15s | Date: %s\n", db->visits[i].visit_id, p->name, db->visits[i].date);
                        count++;
                    }
                }
            }
            if (count == 0) {
                printf("  [INFO] No processed lab reports found to update.\n");
                pauseSystem();
                continue;
            }
            printf("-------------------------------------------\n");

            char v_id[ID_LEN];
            printf("\nEnter Visit ID to Update Lab Report: "); safeInput(v_id, ID_LEN);
            
            VisitRecord *v = NULL;
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].visit_id, v_id) == 0 && strcmp(db->visits[i].lab_id, lab->id) == 0 && db->visits[i].status >= REQ_BILL) {
                    v = &db->visits[i];
                    break;
                }
            }
            
            if (v) {
                char tmp[250];
                int is_updated = 0;
                printf("\n--- UPDATING LAB REPORT (Press Enter to keep current) ---\n");
                
                printf("Current Tests Requested/Conducted: %s\nNew Tests Conducted: ", v->tests_required);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->tests_required, tmp) != 0) { strcpy(v->tests_required, tmp); is_updated = 1; }
                
                printf("Current Official Results: %s\nNew Official Results: ", v->test_results);
                safeInput(tmp, 200); if (strlen(tmp) > 0 && strcmp(v->test_results, tmp) != 0) { strcpy(v->test_results, tmp); is_updated = 1; }
                
                if (is_updated) {
                    saveDatabase(db);
                    exportLabReport(db, v); // Instantly overwrite the text file report
                    writeAuditLog(lab->id, "UPDATED_LAB_REPORT_FULL", v->visit_id);
                    printf("[SUCCESS] Lab report updated & document re-exported.\n");
                } else {
                    printf("[INFO] No changes were made to the lab report.\n");
                }
           } else { 
                printf("[ERROR] Visit ID not found, incomplete, or unauthorized.\n");
           }
            pauseSystem();
        }
        else if (choice == 4) {
            printHeader("SEARCH MY HISTORICAL PATIENTS");
            
            // Show only unique patients that have previously been processed by this specific lab assistant
            printf("--- ASSIGNED PATIENTS HISTORY DIRECTORY ---\n");
            int historyCount = 0;
            
            // Temporary collection check array matching size bounds
            int *alreadyPrinted = (int *)calloc(db->pat_count, sizeof(int));
            
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].lab_id, lab->id) == 0) {
                    for(int j = 0; j < db->pat_count; j++) {
                        if(strcmp(db->patients[j].id, db->visits[i].patient_id) == 0 && db->patients[j].is_active && !alreadyPrinted[j]) {
                            printf("  ID: %-10s | Name: %-15s | Phone: %-15s | Email: %s\n", 
                                   db->patients[j].id, db->patients[j].name, db->patients[j].contact, db->patients[j].email);
                            alreadyPrinted[j] = 1;
                            historyCount++;
                        }
                    }
                }
            }
            free(alreadyPrinted);
            
            if (historyCount == 0) {
                printf("  [INFO] You have not processed records for any patients yet.\n");
                printf("-------------------------------------------\n");
                pauseSystem();
                continue;
            }
            printf("-------------------------------------------\n\n");

            char query[MAX_STR];
            printf("Enter Patient Email or Phone Number from your list: "); safeInput(query, MAX_STR);
            
            Patient *found_pat = NULL;
            // Verify search matches data belonging strictly to this lab assistant's operational sphere
            for (int i = 0; i < db->visit_count; i++) {
                if (strcmp(db->visits[i].lab_id, lab->id) == 0) {
                    Patient *p = getPatientByID(db, db->visits[i].patient_id);
                    if (p && (strcmp(p->email, query) == 0 || strcmp(p->contact, query) == 0)) {
                        found_pat = p;
                        break;
                    }
                }
            }

            if (found_pat) {
                printf("\n[FOUND] ID: %s | Name: %s | Age: %d | Blood: %s\n", 
                       found_pat->id, found_pat->name, found_pat->age, found_pat->blood_group);
                displayPatientHistory(db, found_pat);
            } else {
                printf("[ERROR] No patient matches that data inside your assigned history records.\n");
                pauseSystem();
            }
        }
        else if (choice == 5) { 
            // NEW: Displays all Lab Assistant properties from the struct
            printHeader("MY PROFILE");
            printf("ID: %s\nName: %s\nEmail: %s\nQualification: %s\nSpecialization: %s\nContact Phone: %s\n",
                   lab->id, lab->name, lab->email, lab->qualification, lab->specialization, lab->contact);
            pauseSystem();
        }
        else if (choice == 6) { 
            int keep_updating = 1; 
            
            while (keep_updating) {
                printHeader("UPDATE MY PROFILE");
                printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Contact Phone\n(4) Qualification\n(5) Specialization\n(6) Exit / Cancel\nChoice: ");
                int ch = getValidInt(1, 6); 
                
                char tmp[MAX_STR];
                int is_updated = 0;

                if (ch == 1) {
                    printf("Current Name: %s\n", lab->name); 
                    printf("Enter New Name (Press Enter to keep current): "); safeInput(tmp, sizeof(tmp));
                    if (strlen(tmp) > 0 && strcmp(lab->name, tmp) != 0) { strcpy(lab->name, tmp); is_updated = 1; }
                } else if (ch == 2) {
                    char plainPass[MAX_STR];
                    decryptPasswordHex(lab->password, plainPass); 
                    printf("Current Password: %s\n", plainPass);
                    
                    printf("Enter New Password (Press Enter to keep current): "); safeInput(tmp, sizeof(tmp));
                    if (strlen(tmp) > 0) {
                        char newEnc[MAX_STR];
                        encryptPasswordHex(tmp, newEnc);
                        if (strcmp(lab->password, newEnc) != 0) { strcpy(lab->password, newEnc); is_updated = 1; }
                    }
                } else if (ch == 3) {
                    printf("Current Phone: %s\n", lab->contact); 
                    printf("Enter New Phone (Press Enter to keep current): "); safeInput(tmp, 20);
                    if (strlen(tmp) > 0 && strcmp(lab->contact, tmp) != 0) { strcpy(lab->contact, tmp); is_updated = 1; }
                } else if (ch == 4) {
                    printf("Current Qualification: %s\n", lab->qualification); 
                    printf("Enter New Qualification (Press Enter to keep current): "); safeInput(tmp, MAX_STR);
                    if (strlen(tmp) > 0 && strcmp(lab->qualification, tmp) != 0) { strcpy(lab->qualification, tmp); is_updated = 1; }
                } else if (ch == 5) {
                    printf("Current Specialization: %s\n", lab->specialization); 
                    printf("Enter New Specialization (Press Enter to keep current): "); safeInput(tmp, MAX_STR);
                    if (strlen(tmp) > 0 && strcmp(lab->specialization, tmp) != 0) { strcpy(lab->specialization, tmp); is_updated = 1; }
                } else if (ch == 6) {
                    printf("\n[INFO] Exiting update menu.\n");
                    keep_updating = 0; 
                    break; 
                }
                
                if (is_updated) {
                    saveDatabase(db);
                    writeAuditLog(lab->id, "UPDATED_OWN_PROFILE", lab->id);
                    printf("[SUCCESS] Profile updated successfully.\n");
                } else if (ch != 6) {
                    printf("[INFO] No changes were made.\n");
                }
                
                printf("\nDo you want to change anything else? (1 = Yes, 0 = No): ");
                if (getValidInt(0, 1) == 0) {
                    keep_updating = 0; 
                }
            }
            pauseSystem();
        }   
    } while (choice != 7); 
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
        else if (choice == 4) {
            updateOwnPassword(pat->password);
            saveDatabase(db);
        }
        else if (choice == 5) {
            printHeader("ABOUT US");
            printf("Address: %s\n", CLINIC_ADDRESS);
            printf("Total Medical Professionals on Staff: %d\n", db->doc_count);
            printf("Emergency Contact: 555-0199\n");
            pauseSystem();
        }
    } while (choice != 6);
}



void viewPharmacyDatabase(Database *db) {
    printf("\n--- COMPREHENSIVE PHARMACY SALES DATABASE ---\n");
    printf(" %-10s | %-12s | %-15s | %-18s | %-20s | %-8s | %s\n", 
           "Sale ID", "Patient ID", "Name", "Contact/Email", "Medicines", "Cost", "Date");
    printf("----------------------------------------------------------------------------------------------------------------------\n");
    if (db->pharm_count == 0) {
        printf("  [INFO] No pharmacy sales recorded yet.\n");
    } else {
        for (int i = 0; i < db->pharm_count; i++) {
            char contact_disp[45];
            if (strcmp(db->pharmacy_sales[i].patient_id, "WALK-IN") == 0) {
                snprintf(contact_disp, sizeof(contact_disp), "%s", db->pharmacy_sales[i].contact);
            } else {
                snprintf(contact_disp, sizeof(contact_disp), "%s", db->pharmacy_sales[i].email);
            }
            
            printf(" %-10s | %-12s | %-15.15s | %-18.18s | %-20.20s | $%-.2f | %s\n",
                   db->pharmacy_sales[i].sale_id, db->pharmacy_sales[i].patient_id,
                   db->pharmacy_sales[i].name, contact_disp,
                   db->pharmacy_sales[i].medicines, db->pharmacy_sales[i].cost,
                   db->pharmacy_sales[i].date);
            
            if ((i + 1) % 15 == 0) pauseSystem();
        }
        printf("----------------------------------------------------------------------------------------------------------------------\n");
        printf("Total Recorded Sales: %d\n", db->pharm_count);
    }
    pauseSystem();
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
        printf("5. Register New Patient\n"); 
        printf("6. Manage Directory (View/Delete with Pagination)\n");
        printf("7. Update User Details (Fix Typos)\n");
        printf("8. View System Logs (Audit)\n");
        printf("9. View My Profile\n");       
        printf("10. Update My Profile\n");    
        printf("11. View Pharmacy Sales DB\n");
        printf("12. Logout\nChoice: ");       
        choice = getValidInt(1, 12);

        if (choice == 1) registerAdmin(db);
        else if (choice == 2) registerEmployee(db, admin->id);
        else if (choice == 3) registerDoctor(db, admin->id);
        else if (choice == 4) registerLabAssistant(db, admin->id);
        else if (choice == 5) registerPatient(db, admin->id);
        else if (choice == 6) adminManageDirectory(db);
        else if (choice == 7) adminUpdateUser(db, admin->id);
        else if (choice == 8) {
            printf("\n--- SYSTEM AUDIT LOGS ---\n");
            FILE *f = fopen("audit_log.txt", "r");
            if (f) {
                char line[256];
                while (fgets(line, sizeof(line), f)) printf("%s", line);
                fclose(f);
            } else { printf("No logs found.\n"); }
            pauseSystem();
        }
        else if (choice == 9) { // View Admin Profile
            printHeader("MY PROFILE DETAILS");
            printf("Admin ID      : %s\n", admin->id);
            printf("Full Name     : %s\n", admin->name);
            printf("Email Address : %s\n", admin->email);
            printf("Account Status: %s\n", admin->is_active ? "Active" : "Deactivated");
            pauseSystem();
        }
        else if (choice == 10) { // Update Admin Profile
            printHeader("UPDATE MY PROFILE");
            printf("What would you like to update?\n(1) My Name\n(2) My Password\n(3) Exit / Cancel\nChoice: ");
            int ch = getValidInt(1, 3);
            
            char tmp[MAX_STR];
            int is_updated = 0;
            
            if (ch == 1) {
                printf("Current Name: %s\n", admin->name);
                printf("Enter New Name: "); safeInput(tmp, sizeof(tmp));
                if (strlen(tmp) > 0 && strcmp(admin->name, tmp) != 0) { strcpy(admin->name, tmp); is_updated = 1; }
            } else if (ch == 2) {
                char plainPass[MAX_STR];
                decryptPasswordHex(admin->password, plainPass);
                printf("Current Password: %s\n", plainPass);
                
                printf("Enter New Password: "); safeInput(tmp, sizeof(tmp));
                if (strlen(tmp) > 0) {
                    char newEnc[MAX_STR];
                    encryptPasswordHex(tmp, newEnc);
                    if (strcmp(admin->password, newEnc) != 0) { strcpy(admin->password, newEnc); is_updated = 1; }
                }
            } else if (ch == 3) {
                printf("[INFO] Update operation aborted.\n");
            }
            
            if (is_updated) {
                saveDatabase(db);
                writeAuditLog(admin->id, "UPDATED_OWN_PROFILE", admin->id);
                printf("[SUCCESS] Profile updated successfully.\n");
            } else if (ch != 3) {
                printf("[INFO] No changes were made.\n");
            }
            pauseSystem();
        }
        else if (choice == 11) {
            viewPharmacyDatabase(db);
        }
    } while (choice != 12);
}

// ==========================================
// 21. THE FULLY INTEGRATED MAIN GATEWAY
// ==========================================
void main() {
    Database db;
    // Explicitly nullify all pointers to guarantee Valgrind safety
    db.admins = NULL; db.employees = NULL; db.doctors = NULL;
    db.lab_assts = NULL; db.patients = NULL; db.visits = NULL;
    db.pharmacy_sales = NULL;
    
    // Explicitly zero capacities and counts
    db.admin_count = 0; db.admin_cap = 0; db.emp_count = 0; db.emp_cap = 0;
    db.doc_count = 0; db.doc_cap = 0; db.lab_count = 0; db.lab_cap = 0;
    db.pat_count = 0; db.pat_cap = 0; db.visit_count = 0; db.visit_cap = 0;
    db.pharm_count = 0; db.pharm_cap = 0;

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
        printf("6. Exit & Shutdown Safely\n\n");
        printf("Select Portal: ");
        role = getValidInt(1, 6);

        if (role == 1) {
            Admin *a = loginAdmin(&db);
            if (a) adminDashboard(&db, a);
        } else if (role == 2) {
            Doctor *d = loginDoctor(&db);
            if (d) doctorDashboard(&db, d);
        } else if (role == 3) {
            char em[MAX_STR], pw[MAX_STR], epw[MAX_STR];
            printHeader("RECEPTION PORTAL");
            printf("Enter Email: "); safeInput(em, MAX_STR);
            
            printf("Password (or type 'FORGOT' | Temp Pass: 123): "); safeInput(pw, MAX_STR);
            if (strcmp(pw, "FORGOT") == 0) { forgotPasswordRecovery(&db, 3); continue; }
            
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
            printf("\n[SYSTEM] Commencing safe shutdown...\n");
            saveDatabase(&db);
            freeDatabase(&db); 
            printf("[SYSTEM] Memory cleared. Goodbye.\n");
        }
    } while (role != 6);
}