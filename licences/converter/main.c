#include <stdio.h>
#include <stdlib.h>

int main(const int argc, char** const argv)
{
	int exit_code = EXIT_FAILURE;

	if (argc < 2)
	{
		fputs("Pass path to input file as an argument.\n", stderr);
	}
	else
	{
		FILE* const file = fopen(argv[1], "rb");

		if (file == NULL)
		{
			fputs("Could not open input file for reading.\n", stderr);
		}
		else
		{
			int character;

			while ((character = fgetc(file)) != EOF)
				fprintf(stdout, "'\\x%02X',", character);

			exit_code = EXIT_SUCCESS;

			fclose(file);
		}
	}

	return exit_code;
}
