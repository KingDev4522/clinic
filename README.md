# 🏥 City Multi-Specialty Clinic | Enterprise CLI System

![Language](https://img.shields.io/badge/Language-C-blue.svg)
![Architecture](https://img.shields.io/badge/Architecture-Monolithic-success.svg)
![Database](https://img.shields.io/badge/Database-Flat--File_Relational-orange.svg)
![Security](https://img.shields.io/badge/Security-Hex--XOR_Encryption-red.svg)

An enterprise-grade, memory-safe Command Line Interface (CLI) healthcare management system engineered entirely from scratch in C. Moving beyond basic console applications, this project operates on a custom-built flat-file relational database and features a dynamic state-machine workflow to handle complex end-to-end clinical routing—from patient intake and doctor evaluation to lab diagnostics and automated billing.

Designed with strict memory-management protocols and bulletproof input handling, the system ensures zero-crash stability, Valgrind-safe execution, and concurrent file-write protection.

---

## 👨‍💻 The Team

This ecosystem was architected and developed by:

* **Debjeet Mazumder** – *Founder, Lead Architect & Lead Backend Engineer*
  Conceived the project, designed the core relational logic pipelines, engineered the custom flat-file database engines, built the custom cryptographic security layer, and programmed the overarching memory-safe architecture.
* **Debadrita Baksi** – *Core Backend Engineer*
  Programmed the C-based backend architecture, optimized critical file I/O operations, and executed the dynamic data routing to translate complex workflow blueprints into fully operational, role-specific CLI dashboards.

---

## ⚙️ Tech Stack & Engineering

* **Language:** Standard C
* **Architecture:** Monolithic Console / CLI Application
* **Database Management:** Custom Flat-File Relational Database (CRUD operations via `|` delimited `.txt` files)
* **Security:** Custom Hex-XOR Cryptography & Role-Based Access Control (RBAC)
* **Memory Management:** Strictly managed dynamic allocation (`malloc`/`realloc`) with automated memory teardown sequences.

---

## ✨ Core Features & Technical Highlights

### 1. Custom Relational Database Engine
Instead of relying on standard SQL libraries, the system utilizes a lightweight, highly optimized internal file-handling engine. It parses text files into dynamic RAM structures upon boot and saves them safely upon exit. It features **File Lock Semaphores** (`db_lock.tmp`) to prevent data corruption during concurrent background saves.

### 2. Dynamic State-Machine Routing
Patients are tracked using dynamic clinical state flags (`REQ_DOC` ➔ `REQ_LAB` ➔ `REQ_BILL` ➔ `COMPLETED`). The routing engine automatically pushes patient data to the appropriate department's queue in real-time, preventing bottlenecks and ensuring strict operational sequences.

### 3. Bulletproof Input & Error Handling
Built with a custom `safeInput()` wrapper utilizing `fgets`, the architecture completely neutralizes buffer overflow vulnerabilities native to standard `scanf()` calls. It automatically strips invisible carriage returns (`\r`) and handles edge-case keystrokes to prevent infinite loops.

### 4. Enterprise Security & Auditing
* **Hex-XOR Encryption:** All user credentials across all 5 roles are obfuscated and secured via a custom cryptographic cipher.
* **Invisible Audit Logging:** Every critical action taken by any staff member is tracked via a background engine that writes timestamped events and sequence IDs to an immutable `audit_log.txt` file.

---

## 🏥 Role-Based Access Control (RBAC) Portals

The clinic ecosystem is strictly segmented into 5 distinct, secure portals to ensure data privacy and operational focus:

### 1. 👑 Master Admin
* **Directory Management:** Register new employees, doctors, lab assistants, and patients with auto-generated alphanumeric IDs (e.g., `DOC/001`).
* **Safe Data Handling:** Execute "Soft Deletes" to securely deactivate accounts without corrupting relational histories.
* **System Oversight:** Update user credentials, resolve typos, monitor the global Audit Log, and access the Standalone Pharmacy Sales Database.

### 2. 🩺 Doctor Clinical Portal
* **Queue Management:** Access an auto-updating active queue of assigned patients waiting for evaluation or currently admitted to a ward.
* **Clinical Evaluation:** Input vitals, symptoms, diagnoses, and prescribe medications backed by a real-time **Automated Allergy Warning System**.
* **Dynamic Routing:** Admit patients to hospital wards, execute daily ward rounds, and intelligently route patients directly to the Lab or Billing queues. Modify and re-export past prescriptions seamlessly.

### 3. 🖥️ Employee (Reception & Billing)
* **Intake & Triage:** Handle walk-in registrations and assign patients to Doctors, direct Ward admissions, or independent Lab Diagnostics.
* **Comprehensive Billing:** Process clinical checkouts by dynamically aggregating daily ward bed costs, variable doctor consultation fees, and pharmacy/lab expenses into a final formatted invoice.
* **Pharmacy & Discharges:** Process standalone "Walk-In" pharmacy sales independently of clinical visits, manage hospital discharges, and dynamically manipulate final pharmacy carts based on actual patient purchases.

### 4. 🔬 Lab Diagnostics
* **Test Processing:** Technicians receive real-time test requests and input official medical results into the database.
* **Infinite Testing Loops:** Support for assigning multiple distinct tests to a single patient in a single visit cycle, automatically routing the patient back to the billing queue once all testing concludes.
* **Report Management:** Update historical lab reports and seamlessly re-export formatted files.

### 5. 🏥 Patient Self-Service Kiosk
* **Personal Portal:** A secure interface allowing patients to view their demographic profile and update passwords.
* **Medical Transparency:** Access comprehensive medical history, pulling relational data including attending physician details, lab results, and prescriptions.
* **Financial Tracking:** Track all historical tax invoices (Paid and Pending).

---

## 📄 Automated Document Generation

The system autonomously compiles and exports neatly formatted text documents directly to the local machine based on the current database state. These documents append new records to the top of the file for easy chronological reading:
* **`_Rx.txt`** - Clinical Prescriptions & Doctor's Advice
* **`_LabReport.txt`** - Official Diagnostic Test Results
* **`_Bill.txt`** - Itemized Tax Invoices & Final Checkouts

---

## 📁 Database Structure

The ecosystem generates and maintains the following flat-file databases in the root directory:
* `admin_db.txt` / `emp_db.txt` / `doc_db.txt` / `lab_db.txt` / `patient_db.txt` — Core demographic and credential storage.
* `visits_db.txt` — The master state-machine ledger tracking active and historical clinical interactions.
* `pharmacy_db.txt` — Dedicated ledger for walk-in and registered patient medicine sales.
* `audit_log.txt` — Security and user action tracking.

---
*Built by Debjeet Mazumder & team to demonstrate advanced data structures, algorithmic file handling, and memory-safe enterprise architecture in C.*
