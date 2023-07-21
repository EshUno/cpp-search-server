//#include <iostream>
////#include "search_server.h"
////#include "request_queue.h"
////#include "paginator.h"
////#include "remove_duplicates.h"

#include "process_queries.h"
//using namespace std;

//#include <algorithm>
//#include <cstdlib>
//#include <future>
//#include <map>
//#include <numeric>
//#include <random>
//#include <string>
//#include <vector>
//#include <mutex>
////#include "concurrent_map.h"
////#include "log_duration.h"
////#include "test_framework.h"
//#include <execution>
//#include <string>
//#include <vector>

#include "test_example_functions.h"

int main() {
   TestSearchServer();

    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
        ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const string query = "curly and funny"s;

    auto report = [&search_server, &query] {
        cout << search_server.GetDocumentCount() << " documents total, "s
             << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
    };

    report();
    // однопоточная версия
    search_server.RemoveDocument(5);
    report();
    // однопоточная версия
    search_server.RemoveDocument(execution::seq, 1);
    report();
    // многопоточная версия
    search_server.RemoveDocument(execution::par, 2);
    report();

    return 0;
   //TestParallelFindTopDocuments();
   //TestPerformanceParallelFindTop();
}

