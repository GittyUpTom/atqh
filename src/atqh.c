/* atqh.c -
 *
 * A program to list the 'at' queue in a format that makes
 * sense to a human.
 *
 * NOTE: The SUID permission is set because the program must
 * be executed as root.
 *
 * NOTE: This program uses the POSIX compatable functions
 * 	readdir_r(), localtime_r() and getpwuid_r().
 * See the atqh2.c program for usage of the Non-POSIX
 * 	readdir(), localtime() and getpwuid() functions.
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "atqh.h"

#define TBUFSIZE 50
#define PBUFSIZE 50
#define MAXJOBS 50 /* Maximum of 50 at jobs will be listed. */

/* void get_the_command(char *, char *, int);
char *trim(char *);
char *trim_left(char *);
char *trim_right(char *);
int main(void); */
void get_the_command(char *j_c, char *f_n, int tabit);
char *trim(char *str);
char *trim_left(char *str);
char *trim_right(char *str);
 
int main(int argc, char *argv[])
{
	struct tm jobtime;
	struct tm *jtptr;
	struct tm time_info;
	time_t jt, later; 
	char tbuffer[TBUFSIZE];
	char pwbuffer[PBUFSIZE];
	char queue;
	long unsigned jobno, ctm, time_to_compare, initial_time_to_compare;
	char job_command[256];
	char directory_to_list[] = "/var/spool/at";
	char file_to_read[40];
	DIR *dirname;
	struct dirent FileName;
	struct dirent *result;
	struct stat stat_buf;
	struct passwd pwdat;
	struct passwd *pwdatptr;
	uid_t real_user;
	int fctr = 0;
	int sj_ctr, low_ctr, tabsize;
	int lngst = 0;
	int x = 0;
	int ctr_jtc_ary = 0;
	long unsigned jobs_times_array[MAXJOBS][4];	/* 50 jobs, their times and a pointer to their command. */
	long unsigned sorted_jobs_times[MAXJOBS][4];	/* A sorted copy of the above. */
	char jobs_commands[MAXJOBS][512];		/* Commands for the MAXJOBS jobs */
	char job_owners[MAXJOBS][12];			/* Owners of the jobs */
	char tmp_str[12];				/* Used to determine length of job number */

	if(argc > 1) {
		printf("\n atqh Version %s\n\n", VERSION);
		exit(EXIT_SUCCESS);
	}

	/* Open the directory, make sure it exists and is readable. */
	if ((dirname = opendir(directory_to_list)) == NULL)
	{
		perror("opendir");
		exit(EXIT_FAILURE);
	}

	/* Find out who we really are. */
	real_user = getuid();

#ifdef DEBUG
	 printf("Directory %s is now open for user %d\n", directory_to_list, real_user);
#endif

/* There are two ways, that I know of, to recursively read a directory.
 * The for() way and the while() way are shown here.
 * I'm using the while() way because it's less complicated, but the
 * for() way works just as well.
 */
/*
  	int return_code;
 	for (return_code = readdir_r(dirname, &FileName, &result);
		result != NULL && return_code == 0;
		return_code = readdir_r(dirname, &FileName, &result))
	}
*/
    while (readdir_r(dirname, &FileName, &result) == 0 && result != NULL)
    {
		if (FileName.d_type == DT_REG && isalpha(FileName.d_name[0])) 
		{
			sscanf(FileName.d_name, "%c%5lx%8lx", &queue, &jobno, &ctm);
			jt = ctm*60;

			jobs_times_array[ctr_jtc_ary][0] = jobno;
			jobs_times_array[ctr_jtc_ary][1] = jt;
			jobs_times_array[ctr_jtc_ary][2] = ctr_jtc_ary;

			/* full file name */
			sprintf(file_to_read, "%s/%s", directory_to_list, FileName.d_name);

			lstat(file_to_read, &stat_buf);
			getpwuid_r(stat_buf.st_uid, &pwdat, pwbuffer, sizeof pwbuffer, &pwdatptr);
			lngst = ((int)strlen(pwdat.pw_name) > lngst) ? (int)strlen(pwdat.pw_name) : lngst;

			if (real_user == 0 || stat_buf.st_uid == real_user) {
				fctr++;
#ifdef DEBUG
				printf("%s is owned by %d - %s\n", file_to_read, stat_buf.st_uid, pwdat.pw_name);
#endif
				localtime_r(&jt, &jobtime);
				jtptr = &jobtime;
				strftime(tbuffer, TBUFSIZE, "%a, %b %d, %Y at %I:%M %p", jtptr);
				tabsize = strlen(tbuffer) + 15;
				jobs_times_array[ctr_jtc_ary][3] = (long unsigned)tabsize;

				/* Read the file and get the command to be executed */
				get_the_command(job_command, file_to_read, tabsize);

				strcpy(jobs_commands[ctr_jtc_ary], job_command);
				strcpy(job_owners[ctr_jtc_ary], pwdat.pw_name);
				ctr_jtc_ary += 1;

#ifdef DEBUG
				/* Print individual jobs here, before sorting the array in date/time order. */
				printf("Job %5lu on %s  %s", jobno, tbuffer, job_command);
#endif
			}
		}
	}

	/* Create a future date/time to sort against. */
	/* This just bumps the current date up 5 years */
	later = time(NULL);
	localtime_r(&later, &time_info);
	time_info.tm_year += 5;
	later = mktime(&time_info);

#ifdef DEBUG
	printf("\n------unsorted-----\n");
	for (x=0; x<fctr; x++)
	{
		printf("%lu %lu %lu %s", jobs_times_array[x][0], jobs_times_array[x][1],
		jobs_times_array[x][2], jobs_commands[jobs_times_array[x][2]]);
	}
	printf("-----------\n");
#endif
	/* Sort jobs_times_array on element 1. Job is element 0, time is 1 and job pointer is 2 */
	initial_time_to_compare = later;  /* This is 5 years from today */
	time_to_compare = later;
	low_ctr = 0;
	for(sj_ctr = 0; sj_ctr < fctr; sj_ctr++)
	{
#ifdef DEBUG
		printf("sj_ctr pass # %i\n", sj_ctr);
#endif
		for(x = 0; x < fctr; x++)
		{
#ifdef DEBUG
			printf("Comparing against %lu <----\n", time_to_compare);
#endif
			if( jobs_times_array[x][1] <= time_to_compare )
			{
#ifdef DEBUG
				printf("Rec # %i %lu is less than %lu\n", x, jobs_times_array[x][1], time_to_compare);
#endif
				low_ctr = x;
				time_to_compare = jobs_times_array[x][1];
			}
		}
#ifdef DEBUG
		printf("------> Lowest found was # %i in sj_ctr pass %i\n", low_ctr, sj_ctr);
#endif
		sorted_jobs_times[sj_ctr][0] = jobs_times_array[low_ctr][0];
		sorted_jobs_times[sj_ctr][1] = jobs_times_array[low_ctr][1];
		sorted_jobs_times[sj_ctr][2] = jobs_times_array[low_ctr][2];
		sorted_jobs_times[sj_ctr][3] = jobs_times_array[low_ctr][3];
#ifdef DEBUG
		printf("Rec # %i changed from %lu ", low_ctr, jobs_times_array[low_ctr][1]);
#endif
		/* Make sure this one isn't used again. */
		jobs_times_array[low_ctr][1] = initial_time_to_compare+10;
#ifdef DEBUG
		printf("to %lu\n", jobs_times_array[low_ctr][1]);
#endif
		time_to_compare = later;
    } 
#ifdef DEBUG
	printf("---GOOD ONES?--------\n");
#endif
	for(x = 0; x < fctr; x++)
	{
		jt = sorted_jobs_times[x][1];
		localtime_r(&jt, &jobtime);
		jtptr = &jobtime;
		strftime(tbuffer, TBUFSIZE, "%a, %b %d, %Y at %I:%M %p", jtptr);
		sprintf(tmp_str, "%lu", sorted_jobs_times[x][0]);
		printf("Job %*lu for %-*s on %s  %s", (int)strlen(tmp_str),
			sorted_jobs_times[x][0], lngst, job_owners[sorted_jobs_times[x][2]],
			tbuffer, jobs_commands[sorted_jobs_times[x][2]] );
	}

	if(fctr == 0) puts("\n No jobs were found.");

#ifdef DEBUG
	printf("\n------sorted-----\n");
	for (x=0; x<fctr; x++)
	{
		printf("%lu %lu %lu %s", sorted_jobs_times[x][0], sorted_jobs_times[x][1],
			sorted_jobs_times[x][2], jobs_commands[sorted_jobs_times[x][2]]);
	}
	printf("-----------\n");
#endif
	/* Close the directory and check for errors. */
	if (closedir(dirname) == -1)
	{
		perror("closedir");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

void get_the_command(char *j_c, char *f_n, int tabit)
{
	/* open the file */
	FILE *file = fopen(f_n, "r");

	if (file == NULL)
	{
		strcpy(j_c, "<<-- File Not Found -->>");
	}
	else
	{
		char line[128], temp_line2[128]; /* *line2; */
		int command_search = 0;
		int totcmds = 0;
		/* read the file contents */
		while ( fgets(line, sizeof line, file) != NULL )
		{
			/*  strip white space (fore and aft)from the line */
			char *line2 = trim(line);

			/* find the commands between the DELIMITER's */
			if(strstr(line2, "DELIMITER") != NULL)
			{
				if(command_search == 0)
				{
					command_search = 1;
				} else {
					command_search = 0;
				}
			}
			else
			{
				if(command_search == 1)
				{
					if(totcmds == 0)
					{
						strcpy(j_c, line2);
						totcmds = 1;
					}
					else
					{
						if(strlen(line2) > 1)
						{
							sprintf(temp_line2, "%*s%s", tabit, " ", line2);
							strcat(j_c, temp_line2);
						}
					}
				}
			}
		}
	}
	if(file) fclose(file);
}

char *trim(char *str)
{
	char *out = str;
	out = trim_left(out);
	out = trim_right(out);
	return out;
}

char *trim_left(char *str)
{
	int len = strlen(str);
	if(len == 0 || str[0] != ' ') return str;
//	if(str[0] != ' ' || str[0] == '\0') return str;
	return trim_left(str++);
}

char *trim_right(char *str)
{
	int len = strlen(str) -1;
	if(len == 0 || str[len] != ' ') return str;
	str[strlen(str) - 1] = '\0';
	return trim_right(str);
}
/* vim: set ts=4: */
