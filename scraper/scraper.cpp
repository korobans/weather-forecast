#include <iostream>
#include <string>
#include <curl/curl.h>
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <sqlite3.h>


using namespace std;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
	size_t totalSize = size * nmemb;
	output->append(static_cast<char*>(contents), totalSize);
	return totalSize;
}

string GetWebsiteData(const string& url)
{
	CURL* curl = curl_easy_init();
	string response;

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		CURLcode res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}
	return response;
}

class Table
{
public:
	vector<string> link;
	vector<string> city;
	vector<double> temperature;
	vector<double> latitude;
	vector<double> longitude;
	vector<int> height;

	void addLocation(double lat, double lon, int ht)
	{
		latitude.push_back(lat);
		longitude.push_back(lon);
		height.push_back(ht);
	}
};

void waitingAnimation(atomic<bool>& animationActive) {
	const char frames[] = { '-', '\\', '|', '/' };
	int frameIndex = 0;

	while (animationActive) {
		cout << "\r" << frames[frameIndex++] << flush;
		frameIndex %= sizeof(frames) / sizeof(frames[0]);
		this_thread::sleep_for(chrono::milliseconds(200));
	}

	cout << "\r  \r"; // Clear the last frame
}

int main()
{
	atomic<bool> animationActive(true);
	thread animationThread(waitingAnimation, ref(animationActive));

	string url = "http://www.pogodaiklimat.ru/monitor.php";
	string html_string = GetWebsiteData(url);
	Table weather;
	//cout << html_string << endl;
	xmlInitParser();
	LIBXML_TEST_VERSION
		FILE* dummyStream;
	freopen_s(&dummyStream, "nul", "w", stderr);

	xmlDocPtr doc = htmlReadMemory(html_string.c_str(), html_string.length(), nullptr, nullptr, HTML_PARSE_RECOVER);
	xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
	const xmlChar* xpathExpr = BAD_CAST "//li[contains(@class, 'big-blue-billet__list_link')]/a";
	xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
	xmlNodeSetPtr nodes = xpathObj->nodesetval;

	if (nodes) {
		for (int i = 0; i < nodes->nodeNr; ++i) {
			xmlNodePtr node = nodes->nodeTab[i];
			xmlChar* href = xmlGetProp(node, BAD_CAST "href");
			xmlChar* content = xmlNodeGetContent(node);
			string sName(reinterpret_cast<char*>(content));

			string sHref((char*)href);
			string fullLink = "http://www.pogodaiklimat.ru" + sHref;
			string sCity((char*)content);
			if (sCity != "Ика")
			{
				weather.city.push_back(sCity);
				weather.link.push_back(fullLink);
			}

			if (sName == "Яшкуль") break;

			xmlFree(href);
			xmlFree(content);
		}
	}

	//int e = 0;
	for (string link : weather.link)
	{
		string html_link = GetWebsiteData(link);
		xmlDocPtr doc = htmlReadMemory(html_link.c_str(), html_link.length(), nullptr, nullptr, HTML_PARSE_RECOVER);
		xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
		const xmlChar* xpathExpr = BAD_CAST "//td[@class='green-color'][normalize-space()][1]";
		xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
		xmlNodeSetPtr nodes = xpathObj->nodesetval;

		if (nodes)
		{
			string sTemp;
			for (int i = 0; i < nodes->nodeNr; ++i)
			{
				xmlNodePtr node = nodes->nodeTab[i];
				xmlChar* temperature = xmlNodeGetContent(node);
				sTemp = reinterpret_cast<char*>(temperature);
			}
			double temp = stod(sTemp);
			weather.temperature.push_back(temp);

		}
		xmlFreeDoc(doc);

		size_t latitudePos = html_link.find("широта") + 13;
		size_t longitudePos = html_link.find("долгота") + 15;
		size_t altitudePos = html_link.find("высота над уровнем моря") + 44;

		string latitude = html_link.substr(latitudePos, html_link.find(",", latitudePos) - latitudePos);
		string longitude = html_link.substr(longitudePos, html_link.find(",", longitudePos) - longitudePos);
		string altitude = html_link.substr(altitudePos, html_link.find(" м.", altitudePos) - altitudePos);

		double dLatitude = stod(latitude);
		double dLongitude = stod(longitude);
		int iAltitude = stoi(altitude);

		weather.addLocation(dLatitude, dLongitude, iAltitude);
		//cout << weather.city[e] << " " << weather.temperature[e] << " " << weather.latitude[e] << " " << weather.longitude[e] << " " << weather.height[e] << endl;
		//e++;
	}

	animationActive = false;
	animationThread.join();

	sqlite3* db;

	int rc = sqlite3_open("weather_statistic.db", &db);

	char* errMsg;
	string clearTableQuery = "DELETE FROM table_data;";
	rc = sqlite3_exec(db, clearTableQuery.c_str(), nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK)
	{
		cerr << "Ошибка при очистке таблицы: " << errMsg << endl;
		sqlite3_free(errMsg);
		return 1;
	}

	string createTableQuery = "CREATE TABLE IF NOT EXISTS table_data ("
		"link TEXT, "
		"city TEXT, "
		"temperature REAL, "
		"latitude REAL, "
		"longitude REAL, "
		"height INTEGER);";

	rc = sqlite3_exec(db, createTableQuery.c_str(), nullptr, nullptr, &errMsg);
	string insertQuery = "INSERT OR REPLACE INTO table_data (link, city, temperature, latitude, longitude, height) VALUES (?, ?, ?, ?, ?, ?);";
	sqlite3_stmt* stmt;

	for (size_t i = 0; i < weather.link.size(); ++i)
	{
		rc = sqlite3_prepare_v2(db, insertQuery.c_str(), -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			cerr << "Ошибка при подготовке запроса: " << sqlite3_errmsg(db) << endl;
			return 1;
		}

		sqlite3_bind_text(stmt, 1, weather.link[i].c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 2, weather.city[i].c_str(), -1, SQLITE_STATIC);
		sqlite3_bind_double(stmt, 3, weather.temperature[i]);
		sqlite3_bind_double(stmt, 4, weather.latitude[i]);
		sqlite3_bind_double(stmt, 5, weather.longitude[i]);
		sqlite3_bind_int(stmt, 6, weather.height[i]);

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
		{
			cerr << "Ошибка при выполнении запроса: " << sqlite3_errmsg(db) << endl;
			return 1;
		}

		sqlite3_finalize(stmt);
	}

	cout << "Данные успешно добавлены в таблицу." << endl;

	sqlite3_close(db);

	return 0;

	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;
}
