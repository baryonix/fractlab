#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <arpa/inet.h>

#include "crc.h"


struct mhdr {
	uint32_t width, height, tps, layers, frames, play_time, profile;
};


static void
write_chunk (FILE *f, const char *chunk_type, const char *data, size_t len, uint32_t *u_crc)
{
	uint32_t the_crc = 0xffffffff;
	if (u_crc == NULL) {
		the_crc = update_crc (the_crc, (const unsigned char *) chunk_type, 4);
		if (len > 0)
			the_crc = update_crc (the_crc, (const unsigned char *) data, len);
		the_crc = ~the_crc;
		the_crc = htonl (the_crc);
	} else
		the_crc = htonl (*u_crc);
	uint32_t the_len = htonl (len);
	fwrite (&the_len, sizeof (the_len), 1, f);
	fwrite (chunk_type, 4, 1, f);
	if (len > 0)
		fwrite (data, 1, len, f);
	fwrite (&the_crc, sizeof (the_crc), 1, f);
}


static long
read_chunk (FILE *f, char *chunk_type, char **data, uint32_t *r_crc, int checkcrc)
{
	uint32_t len, the_crc, my_crc = 0;
	char *buf = NULL;
	if (fread (&len, sizeof (len), 1, f) < 1)
		goto error;
	len = ntohl (len);
	if (fread (chunk_type, 4, 1, f) < 1)
		goto error;
	if (checkcrc) {
		my_crc = 0xffffffff;
		my_crc = update_crc (my_crc, (unsigned char *) chunk_type, 4);
	}
	if (len > 0) {
		buf = malloc (len);
		if (buf == NULL)
			goto error;
		if (fread (buf, 1, len, f) < len)
			goto error;
		if (checkcrc)
			my_crc = update_crc (my_crc, (unsigned char *) buf, len);
	}
	if (fread (&the_crc, sizeof (the_crc), 1, f) < 1)
		goto error;
	the_crc = ntohl (the_crc);
	my_crc = ~my_crc;
	if (checkcrc && the_crc != my_crc) {
		fprintf (stderr, "wrong crc\n");
		goto error;
	}
	if (len > 0)
		*data = buf;
	if (r_crc != NULL)
		*r_crc = the_crc;
	return len;

 error:
	if (buf != NULL)
		free (buf);
	return -1;
}


static void
usage (const char *argv0)
{
	fprintf (stderr, "USAGE: %s [-h] -n frames -W pixels -H pixels [-r fps] -F format -o outfile [-C]\n", argv0);
	fprintf (stderr, "\n");
	fprintf (stderr, "  Combine a bunch of PNG files into a MNG-VLC (Very Low Complexity) animation.\n");
	fprintf (stderr, "\n");
	fprintf (stderr, "    -h      print this help message\n");
	fprintf (stderr, "    -n      # of frames in animation (first frame has number 0)\n");
	fprintf (stderr, "    -W      image width\n");
	fprintf (stderr, "    -H      image height\n");
	fprintf (stderr, "    -r      frame rate (default 25 fps)\n");
	fprintf (stderr, "    -F      format string to generate input file names\n");
	fprintf (stderr, "            It must take a single argument of type long, e.g. \"file%%06ld.png\"\n");
	fprintf (stderr, "            The format string is not checked for validity!\n");
	fprintf (stderr, "    -o      output file\n");
	fprintf (stderr, "    -C      don't check CRC of PNG chunks\n");
	fprintf (stderr, "            This will speed up the conversion a bit, but is generally\n");
	fprintf (stderr, "            not necessary -- CRC32 is very fast.\n");
}


int
main (int argc, char *argv[])
{
	long frames = 0, width = 0, height = 0, rate = 25;
	const char *format = NULL, *outfile = NULL;
	int opt, checkcrc = 1;
	opterr = 0;
	while ((opt = getopt (argc, argv, "h?n:W:H:r:F:o:C")) != -1) {
		switch (opt) {
			case 'n': {
				frames = strtol (optarg, NULL, 10);
				break;
			}
			case 'W': {
				width = strtol (optarg, NULL, 10);
				break;
			}
			case 'H': {
				height = strtol (optarg, NULL, 10);
				break;
			}
			case 'F': {
				format = optarg;
				break;
			}
			case 'r': {
				rate = strtol (optarg, NULL, 10);
				break;
			}
			case 'o': {
				outfile = optarg;
				break;
			}
			case 'C': {
				checkcrc = 0;
				break;
			}
			case 'h':
			case '?': {
				usage (argv[0]);
				return 1;
			}
		}
	}
	if (optind < argc || frames <= 0 || width <= 0 || height <= 0 || format == NULL || outfile == NULL) {
		usage (argv[0]);
		return 1;
	}

	FILE *out = fopen (outfile, "wb");
	if (out == NULL) {
		fprintf (stderr, "%s: cannot open: %s\n", outfile, strerror (errno));
		return 1;
	}
	if (fwrite ("\212MNG\r\n\032\n", 8, 1, out) < 1) {
		fprintf (stderr, "%s: cannot write signature: %s\n", outfile, strerror (errno));
		return 1;
	}
	struct mhdr mhdr;
	mhdr.width = htonl (width);
	mhdr.height = htonl (height);
	mhdr.tps = htonl (rate);
	mhdr.layers = htonl (frames + 1);
	mhdr.frames = htonl (frames);
	mhdr.play_time = htonl (frames);
	mhdr.profile = htonl (0x41);
	write_chunk (out, "MHDR", (char *) &mhdr, sizeof (mhdr), NULL);

	char bg[6] = {0, 0, 0, 0, 0, 0};
	write_chunk (out, "BACK", bg, sizeof (bg), NULL);

	long i;
	for (i = 0; i < frames; i++) {
		char fname[1024];
		sprintf (fname, format, i);
		FILE *f = fopen (fname, "rb");
		if (f == NULL) {
			fprintf (stderr, "%s: cannot open: %s\n", fname, strerror (errno));
			return 1;
		}
		char sig[8];
		if (fread (sig, 8, 1, f) < 1) {
			fprintf (stderr, "%s: cannot read signature: %s\n", fname, strerror (errno));
			return 1;
		}
		if (memcmp (sig, "\211PNG\r\n\032\n", 8) != 0) {
			fprintf (stderr, "%s: not a PNG file\n", fname);
			return 1;
		}
		char chunk_type[4];
		uint32_t the_crc;
		long len;
		while (1) {
			char *buf = NULL;
			len = read_chunk (f, chunk_type, &buf, &the_crc, checkcrc);
			if (len < 0) {
				fprintf (stderr, "%s: read error!\n", fname);
				return 1;
			}
			write_chunk (out, chunk_type, buf, len, &the_crc);
			if (len > 0)
				free (buf);
			if (memcmp (chunk_type, "IEND", 4) == 0)
				break;
		}
		fclose (f);
	}
	write_chunk (out, "MEND", NULL, 0, NULL);
	fclose (out);
	return 0;
}
