#include <iostream>
#include <curl/curl.h>
#include "libxml/HTMLparser.h"
#include "libxml/xpath.h"
#include <vector>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string GetWebsiteData(const std::string& url)
{
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cerr << "Ошибка при выполнении запроса: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

int main()
{
    std::string url = "http://www.pogodaiklimat.ru/monitor.php";
    std::string html_string = GetWebsiteData(url);

    //std::cout << html_string << std::endl;

    xmlInitParser();
    LIBXML_TEST_VERSION
#ifdef _WIN32
        FILE* dummyStream;
    freopen_s(&dummyStream, "nul", "w", stderr);
#else
        freopen("/dev/null", "w", stderr);
#endif
        xmlDocPtr doc = htmlReadMemory(html_string.c_str(), html_string.length(), nullptr, nullptr, HTML_PARSE_RECOVER);

    if (doc == nullptr) {
        std::cerr << "Error parsing HTML string." << std::endl;
        xmlCleanupParser();
        return 1;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);

    const xmlChar* xpathExpr = BAD_CAST "//li[contains(@class, 'big-blue-billet__list_link')]/a";

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);

    xmlNodeSetPtr nodes = xpathObj->nodesetval;
    if (nodes) {
        for (int i = 0; i < nodes->nodeNr; ++i) {
            xmlNodePtr node = nodes->nodeTab[i];
            xmlChar* href = xmlGetProp(node, BAD_CAST "href");
            xmlChar* content = xmlNodeGetContent(node);
            std::string sName(reinterpret_cast<char*>(content));

            std::cout << "Гиперссылка: " << href << std::endl;
            std::cout << "Имя: " << content << std::endl;

            if (sName == "Яшкуль") break;

            xmlFree(href);
            xmlFree(content);
        }
    }

    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return 0;
}