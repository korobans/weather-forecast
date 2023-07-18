#include <iostream>
#include <vector>
#include <cmath>
#include <sqlite3.h>

#define R 6371.0 // Радиус Земли в километрах
#define M_PI 3.14159265358979323846

using namespace std;

double toRadians(double degrees) {
    return degrees * M_PI / 180.0;
}

double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);

    double a = sin(dLat / 2) * sin(dLat / 2) +
        cos(toRadians(lat1)) * cos(toRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);

    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    double distance = R * c;

    return distance;
}

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

        city.push_back(string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
        temperature.push_back(sqlite3_column_double(stmt, 1));
        latitude.push_back(sqlite3_column_double(stmt, 2));
        longitude.push_back(sqlite3_column_double(stmt, 3));
        height.push_back(sqlite3_column_double(stmt, 4));

    }

    cout << "Введите широту и долготу через пробел : ";
    cin >> x;
    cin >> y;


    cout << endl;


    for (int i = 0; i < city.size(); i++)
    {
        double distance = calculateDistance(x, y, latitude[i], longitude[i]);
        if (distance < 500)
        {
            vector<double> row = { temperature[i], latitude[i], longitude[i] , distance};
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
