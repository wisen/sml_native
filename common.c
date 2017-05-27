#include <sys/stat.h>
#include <errno.h>

#include "headers/common.h"

//isblock_fromX: 1 block from ui; 2 block from self
static int isblock_fromX = 0;

void set_blockflag (int flag)
{
	isblock_fromX = flag;
}
int get_blockflag()
{
	return isblock_fromX;
}

int dir_exists (const char *path)
{
	struct stat st;

	if (stat (path, & st) < 0)
	{
		DM ("stat [%s]: %s\n", path, strerror (errno));
		return 0;
	}

	return (S_ISDIR (st.st_mode) ? 1 : 0);
}

int dir_create_recursive (const char *path)
{
	char *pbuf;

	if (dir_exists (path))
		return 0;

	pbuf = strdup (path);

	if (pbuf)
	{
		char *p;

		p = & pbuf [strlen (pbuf) - 1];

		if (*p == '/') *p = 0;

		p = pbuf;

		if (*p == '/') p ++;

		for (p = strchr (p, '/'); p; p = strchr (p, '/'))
		{
			*p = 0;

			if (mkdir (pbuf, DEFAULT_DIR_MODE) < 0)
			{
				if (errno == EEXIST)
				{
					/* do not show EEXIST error here unless it's not a directory */
					if (! dir_exists (pbuf))
					{
						DM ("mkdir [%s]: path was existed but not a directory! force removing the invalid file and try again...\n", pbuf);

						unlink_file (pbuf);

						mkdir (pbuf, DEFAULT_DIR_MODE);
					}
				}
				else
				{
					DM ("mkdir [%s]: %s\n", path, strerror (errno));
				}
			}

			*p ++ = '/';
		}

		free (pbuf);
	}

	if (mkdir (path, DEFAULT_DIR_MODE) < 0)
	{
		if (errno == EEXIST)
		{
			if (! dir_exists (path))
			{
				DM ("mkdir [%s]: path was existed but not a directory! force removing the invalid file and try again...\n", path);

				unlink_file (path);

				if (mkdir (path, DEFAULT_DIR_MODE) < 0)
				{
					DM ("mkdir retried [%s]: %s\n", path, strerror (errno));
				}
			}
			else
			{
				DM ("mkdir [%s]: %s\n", path, strerror (errno));
			}
		}
		else
		{
			DM ("mkdir [%s]: %s\n", path, strerror (errno));
		}
	}
	else
	{
		DM ("mkdir [%s] ok.\n", path);
	}

	return (dir_exists (path) ? 0 : -1);
}

int unlink_file (const char *path)
{
	char *pbuf;
	char *p;
	int ret, err;

	if (path [strlen (path) - 1] != '/')
		return unlink (path);

	pbuf = strdup (path);

	if (! pbuf)
		return unlink (path);

	for (p = & pbuf [strlen (pbuf) - 1]; (p != pbuf) && (*p == '/'); p --)
		*p = 0;

	ret = unlink (pbuf);
	err = errno;

	free (pbuf);

	errno = err;
	return ret;
}

int str_replace (uint8_t *orig, uint8_t *repl, int len)
{
	int origIdx = 0;
	int replIdx = 0;
	for (origIdx = 0; origIdx < len ; ++origIdx )
	{
		if (orig [origIdx] == '\r')
		{
			continue;
		}
		else if (orig [origIdx] == '\n')
		{
			repl [replIdx++] = '\\';
			repl [replIdx++] = 'n';
			repl [replIdx++] = '\\';
			repl [replIdx++] = '\n';
		}
		else
		{
			repl [replIdx++] = orig [origIdx];
		}
	}
	return replIdx;
}
