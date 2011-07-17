/* Every time the Redislite Git SHA1 or Dirty status changes only this file
 * small file is recompiled, as we access this information in all the other
 * files using this functions. */

#include "release.h"

char *redislite_git_SHA1()
{
	return REDISLITE_GIT_SHA1;
}

char *redislite_git_dirty()
{
	return REDISLITE_GIT_DIRTY;
}

