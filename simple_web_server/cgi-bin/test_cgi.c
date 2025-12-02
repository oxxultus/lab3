#include <stdio.h>
#include <stdlib.h>

int main() {
    char *query_string = getenv("QUERY_STRING");

    printf("Content-type: text/html\r\n\r\n"); // CGI 헤더 (중요)
    printf("<html><head><title>CGI Test Result</title></head><body>");
    printf("<h1>CGI POST Request Received</h1>");
    
    if (query_string) {
        printf("<p>Query String (POST Data): <strong>%s</strong></p>", query_string);
    } else {
        printf("<p>No Query String Received.</p>");
    }
    
    printf("</body></html>");
    
    return 0;
}