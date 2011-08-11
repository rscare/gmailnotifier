#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libnotify/notify.h>
#include "gmailxml.h"

time_t last_update = 0;

time_t notifyEntry(xmlNode *node) {
    xmlNode *tmp;
    xmlChar *title = NULL, *text = NULL;
    xmlChar *author_name = NULL, *author_email = NULL;

    char *notification_title = NULL, *notification_summary = NULL;
    size_t len;

    char *update_time_str = NULL;
    time_t update_time;
    struct tm time_struct;

    NotifyNotification *notification;
    GError *error = NULL;

    node = node->children;
    while (node != NULL) {
        if (xmlStrcmp(node->name, (const xmlChar *)"title") == 0)
            title = node->children->content;
        else if (xmlStrcmp(node->name, (const xmlChar *)"summary") == 0)
            text = node->children->content;
        else if (xmlStrcmp(node->name, (const xmlChar *)"modified") == 0)
            update_time_str = (char *)(node->children->content);
        else if (xmlStrcmp(node->name, (const xmlChar *)"author") == 0) {
            tmp = node->children;
            while (tmp != NULL) {
                if (xmlStrcmp(tmp->name, (const xmlChar *)"name") == 0)
                    author_name = tmp->children->content;
                else if (xmlStrcmp(tmp->name, (const xmlChar *)"email") == 0)
                    author_email = tmp->children->content;
                tmp = tmp->next;
            }
        }
        node = node->next;
    }

    strptime(update_time_str, "%Y-%m-%dT%T%z", &time_struct);
    update_time = timegm(&time_struct);

    if (difftime(update_time, last_update) >= 0) {
        // Run the notification
        len = strlen(author_name) + strlen(author_email) + 4;
        notification_title = malloc(len);
        snprintf(notification_title, len, "%s <%s>", author_name, author_email);

        len = strlen(title) + strlen(text) + 4;
        notification_summary = malloc(len);
        snprintf(notification_summary, len, "%s - %s", title, text);

        notification = notify_notification_new(notification_title,
                                               notification_summary,
                                               MAIL_NOTIFICATION_ICON);
        notify_notification_show(notification, &error);
        g_object_unref(G_OBJECT(notification));

        free(notification_title);
        free(notification_summary);
    }
    return update_time;
}

unsigned short int notify_New_Emails(const char * str, const char *url) {
    xmlDoc *doc = NULL;
    xmlNode *node = NULL;
    unsigned short int new_msgs = 0;
    time_t tmptime = 0, largesttime;

    largesttime = last_update;

    LIBXML_TEST_VERSION;

    doc = xmlReadMemory(str, strlen(str), url, NULL,
                        XML_PARSE_NOERROR|XML_PARSE_NOWARNING);

    if (doc == NULL) {
        printf("Failed to open file...\n");
        return 0;
    }

    node = xmlDocGetRootElement(doc);
    if (node == NULL) {
        printf("Failed to find root element...\n");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return 0;
    }
    node = node->children;
    if (node == NULL ) {
        printf("Empty feed...\n");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return 0;
    }

    while (node != NULL) {
        if (xmlStrcmp(node->name, (const xmlChar *)"entry") == 0) {
            tmptime = notifyEntry(node);
            if (tmptime > largesttime)
                largesttime = tmptime;
        }
        else if (xmlStrcmp(node->name, (const xmlChar *)"fullcount") == 0)
            new_msgs = (unsigned short int)(atoi((char *)(node->children->content)));

        node = node->next;
    }
    last_update = largesttime;

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return new_msgs;
}
