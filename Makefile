CC = gcc
DEBUG = -g
# CC += $(DEBUG)

all: gmailnotifier gmail_new_mails

gmail_new_mails: gmail_new_mails.c
	$(CC) gmail_new_mails.c -o gmail_new_mails

gmailnotifier: gmailnotifier.o rpass.o gmailxml.o
	$(CC) gmailnotifier.o rpass.o gmailxml.o -lcurl `xml2-config --libs` `gpgme-config --libs` `pkg-config --libs libnotify` -o gmailnotifier

gmailxml.o: gmailxml.c gmailxml.h
	$(CC) -c gmailxml.c `xml2-config --cflags` `pkg-config --cflags libnotify` -o gmailxml.o

gmailnotifier.o: gmailnotifier.c
	$(CC) -c gmailnotifier.c `xml2-config --cflags` `gpgme-config --cflags` `pkg-config --cflags libnotify` -o gmailnotifier.o

rpass.o: rpass.c rpass.h
	$(CC) -c rpass.c `gpgme-config --cflags` -o rpass.o

clean:
	rm *.o gmailnotifier gmail_new_mails
