#include <iostream>
#include <list>
#include <sqlite3.h>

int main() {
    sqlite3* db;
    int rc = sqlite3_open("data.sqlite", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return rc;
    }

    sqlite3_stmt* stmt;
    const char* query = "SELECT * FROM weather";
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return rc;
    }


    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* column2 = sqlite3_column_text(stmt, 1);
        double column4 = sqlite3_column_double(stmt, 3);
        double column5 = sqlite3_column_double(stmt, 4);

        std::cout << column2 << " - ш. " << column4 << " д. " << column5 << std::endl;
    }


    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}
