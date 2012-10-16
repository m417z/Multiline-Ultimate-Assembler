#include "plugin.h"

BOOL MyGetintfromini(wchar_t *key, int *p_val, int min, int max, int def)
{
	if(Getfromini(NULL, DEF_PLUGINNAME, key, L"%i", p_val))
	{
		if(min && max && (*p_val < min || *p_val > max))
			*p_val = def;

		return TRUE;
	}

	*p_val = def;
	return FALSE;
}

BOOL MyWriteinttoini(wchar_t *key, int val)
{
	return (Writetoini(NULL, DEF_PLUGINNAME, key, L"%i", val) != 0);
}

int MyGetstringfromini(wchar_t *key, wchar_t *s, int length)
{
	return Stringfromini(DEF_PLUGINNAME, key, s, length);
}

int MyWritestringtoini(wchar_t *key, wchar_t *s)
{
	return Writetoini(NULL, DEF_PLUGINNAME, key, L"%s", s);
}
