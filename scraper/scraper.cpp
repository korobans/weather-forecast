#include <iostream>
#include <cstdio>
#include <string>
#include <curl/curl.h>
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <sqlite3.h>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

using namespace std;

// хз спизженный кусок кода
size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
	size_t totalSize = size * nmemb;
	output->append(static_cast<char*>(contents), totalSize);
	return totalSize;
}

// получение верстки сайта
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

// класс таблицы
class Table
{
public:
	// объявление массивов с данными метеостанций
	vector<string> link;
	vector<string> city;
	vector<double> temperature;
	vector<double> latitude;
	vector<double> longitude;
	vector<int> height;

	void addLocation(double lat, double lon, int ht) //заполнение координат
	{
		latitude.push_back(lat);
		longitude.push_back(lon);
		height.push_back(ht);
	}
};

//анимация вращения палки :)
void waitingAnimation(atomic<bool>& animationActive) {
	const char frames[] = { '-', '\\', '|', '/' };
	int frameIndex = 0;

	while (animationActive) {
		cout << "\r" << frames[frameIndex++] << flush;
		frameIndex %= sizeof(frames) / sizeof(frames[0]);
		this_thread::sleep_for(chrono::milliseconds(200));
	}

	cout << "\r  \r";
}

int main()
{
	// запуск анимации палки
	atomic<bool> animationActive(true);
	thread animationThread(waitingAnimation, ref(animationActive));

	// получение верстки сайта с погодой
	string url = "http://www.pogodaiklimat.ru/monitor.php";
	string html_string = GetWebsiteData(url);

	// создание таблицы
	Table weather;


	// подключение парсера
	xmlInitParser();
	LIBXML_TEST_VERSION
		FILE* dummyStream;
	freopen_s(&dummyStream, "nul", "w", stderr);

	// парсинг нашей верстки
	xmlDocPtr doc = htmlReadMemory(html_string.c_str(), html_string.length(), nullptr, nullptr, HTML_PARSE_RECOVER);
	xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);

	// поиск по верстке элементов списка класса big-blue-billet__list_link
	const xmlChar* xpathExpr = BAD_CAST "//li[contains(@class, 'big-blue-billet__list_link')]/a";
	xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
	xmlNodeSetPtr nodes = xpathObj->nodesetval; //запись полученных элементов в nodes

	if (nodes) { // проверка на заполненость дерева
		for (int i = 0; i < nodes->nodeNr; ++i) { // прохождение по массиву циклом
			xmlNodePtr node = nodes->nodeTab[i];
			xmlChar* href = xmlGetProp(node, BAD_CAST "href"); // извлечение id города
			xmlChar* content = xmlNodeGetContent(node); // получение названия города
			string sName(reinterpret_cast<char*>(content));

			string sHref((char*)href);
			string fullLink = "http://www.pogodaiklimat.ru" + sHref; // генерация ссылки с информацией по городу
			string sCity((char*)content); // запись названия города
			if (sCity != "Ика") // город ика не содержит внутри себя данных, ломая код, поэтому его пропускаем
			{
				//добавление в массив информации
				weather.city.push_back(sCity);
				weather.link.push_back(fullLink);
			}

			if (sName == "Яшкуль") break; // проверка на последний город

			// очистка памяти
			xmlFree(href);
			xmlFree(content);
		}
	}

	// парсинг всех сайтов с данными о российских метеовышках
	for (string link : weather.link)
	{
		// получение со сгенерированной ссылки информации о средней сегодняшней температуры
		string html_link = GetWebsiteData(link);
		xmlDocPtr doc = htmlReadMemory(html_link.c_str(), html_link.length(), nullptr, nullptr, HTML_PARSE_RECOVER);
		xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
		const xmlChar* xpathExpr = BAD_CAST "//td[@class='green-color'][normalize-space()][1]";
		xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
		xmlNodeSetPtr nodes = xpathObj->nodesetval;

		if (nodes)
		{
			// обработка и запись информации о температуре
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

		// поиск вхождения в подстроку с информацией о расположении вышки
		size_t latitudePos = html_link.find("широта") + 13;
		size_t longitudePos = html_link.find("долгота") + 15;
		size_t altitudePos = html_link.find("высота над уровнем моря") + 44;

		// запись в строку найденной информации
		string latitude = html_link.substr(latitudePos, html_link.find(",", latitudePos) - latitudePos);
		string longitude = html_link.substr(longitudePos, html_link.find(",", longitudePos) - longitudePos);
		string altitude = html_link.substr(altitudePos, html_link.find(" м.", altitudePos) - altitudePos);

		// превращение строки в другие типы данных
		double dLatitude = stod(latitude);
		double dLongitude = stod(longitude);
		int iAltitude = stoi(altitude);

		weather.addLocation(dLatitude, dLongitude, iAltitude); // запись координат
	}

	// остановка анимации
	animationActive = false;
	animationThread.join();

	// удаление прошлой бд
	const char* filename = "weather_statistic.db";
	remove(filename);

	// подключение SQLite3 и создание новой бд
	sqlite3* db;
	int rc = sqlite3_open("weather_statistic.db", &db);

	// создание шаблона таблицы
	char* errMsg;
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
	// заполнение таблицы
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

	// очистка памяти
	xmlXPathFreeObject(xpathObj);
	xmlXPathFreeContext(xpathCtx);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;
}
