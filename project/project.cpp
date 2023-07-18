#include <iostream>
#include <vector>
#include <cmath>
#include <sqlite3.h>

using namespace std;


int main() 
{
    vector<string> city;
    vector<double> temperature;
    vector<double> latitude;
    vector<double> longitude;
    vector<double> height;
    vector<double> length;
    vector<vector<double>> near;
    double x, y;

    sqlite3* db;
    int rc = sqlite3_open("data.sqlite", &db);
    if (rc != SQLITE_OK) 
    {
        cerr << "Cannot open database: " << sqlite3_errmsg(db) << endl;
        return rc;
    }

    sqlite3_stmt* stmt;
    const char* query = "SELECT * FROM weather";
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) 
    {
        cerr << "Cannot prepare statement: " << sqlite3_errmsg(db) << endl;
        return rc;
    }


    while (sqlite3_step(stmt) == SQLITE_ROW) 
    {

        city.push_back(string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))));
        temperature.push_back(sqlite3_column_double(stmt, 2));
        latitude.push_back(sqlite3_column_double(stmt, 3));
        longitude.push_back(sqlite3_column_double(stmt, 4));
        height.push_back(sqlite3_column_double(stmt, 5));

    }

    cout << "Введите широту и долготу через пробел : ";
    cin >> x;
    cin >> y;


    cout << endl;


    for (int i = 0; i < city.size(); i++)
    {
        double distance = sqrt(pow(x-latitude[i], 2.0)+pow(y-longitude[i], 2.0)) * 111;
        if (distance < 500)
        {
            vector<double> row = { temperature[i], latitude[i], longitude[i] };
            near.push_back(row);
        }

    }

    for (vector<double> i : near)
    {
        for (double e : i)
        {
            cout << e << endl;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}
