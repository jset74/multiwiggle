/*
 
Author: Eugene Kulesha 
Contact: kulesh@gmail.com

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h> // atoi, atol
#include <dirent.h> // readdir
#include <sys/stat.h> // mkdir, stat

#include <error.h>
#include <errno.h>

#include <unistd.h> //chdir

#include "mw.h"

#define MAX_PATH 1024

#define RFILE "MW.regions"
#define TFILE "MW.tracks"


TRECORD *troot = NULL, *tcur = NULL;
RRECORD *rroot = NULL, *rcur = NULL;
static int verbose = 0;

int create_index(char *workdir,  char *fname, UINT taxon, char *assembly, char *desc ) {
  HEADER h;
  char hfile[250];
  FILE *hf;
  char cmd[1024];
  VALUE minV, maxV;
  FILE *th, *rf;
  char *line = NULL;
  size_t len = 0;

  TRACK a;
  REGION r;


  printf("Creating MW file ...\n");

  chdir(workdir);
  sprintf(hfile, "%s.head", "test01");

  sprintf(h.format, FORMAT_MWIG);
  h.v_major = VERSION_MAJOR;
  h.v_minor = VERSION_MINOR;
  h.taxon = taxon;
  sprintf(h.assembly, assembly);
  sprintf(h.desc, desc);
  h.tracks = 0;
  h.regions = 0;

  if ((th = fopen(TFILE, "r"))) {
    while(getline(&line, &len, th) > 0 ) {
      if (sscanf(line, "%u %f %f %u %s %s", &a.id, &a.min, &a.max, &a.ori, a.name, a.desc) > 4) {
	if (!h.tracks) {
	  minV = a.min;
	  maxV = a.max;
	} else {
	  if (minV > a.min) {
	    minV = a.min;
	  }
	  if (maxV < a.max) {
	    maxV = a.max;
	  }
	}
	h.tracks++;
      }
    }    
    fclose(th);
  }

  if (th = fopen(RFILE, "r")) {
    while(getline(&line, &len, th) > 0 ) {
      if (sscanf(line, "%s %ld %ld", &r.name, &r.offset, &r.size) == 3) {
	h.regions++;
      }
    }    
    fclose(th);
  }

  h.min = minV;
  h.max = maxV;

  h.offset = sizeof(HEADER) + sizeof(TRACK) * h.tracks + sizeof(REGION) * h.regions;
  //  printf("\nFormat: %s\nVersion: %d.%d\nTracks: %d\nRegions: %d\nOffset: %ld\nTaxon ID: %d\nAssembly: %s\nValues: %f .. %f\n\n", h.format, h.v_major, h.v_minor, h.tracks, h.regions, h.offset, h.taxon, h.assembly, h.min, h.max);


  if ((hf = fopen(hfile,"w+"))) {
    fwrite(&h, sizeof(HEADER), 1, hf);

    if (th = fopen(TFILE, "r")) {
      while(getline(&line, &len, th) > 0 ) {
	if (sscanf(line, "%d %f %f %d %s %s", &a.id, &a.min, &a.max, &a.ori, a.name, a.desc) > 4) {
	  fwrite(&a, sizeof(TRACK), 1, hf);
	}
      }    
      fclose(th);
    }

    if (th = fopen(RFILE, "r")) {
      while(getline(&line, &len, th) > 0 ) {
	if (sscanf(line, "%s %ld %ld", &r.name, &r.offset, &r.size) == 3) {
	  fwrite(&r, sizeof(REGION), 1, hf);
	}
      }    
      fclose(th);
    }
    fclose(hf);
  }

  if (line) {
    free(line);
  }

  sprintf(cmd, "cat test01.head test01.body > %s", fname);
  system(cmd);

  if (verbose < 100) {
    sprintf(cmd, "rm -rf %s ", workdir);
    system(cmd);
  }
  
  return 0;
}

int merge_regions (char* workdir,  int total ) {
  struct dirent *e;
  DIR *d = opendir(".");
  FILE *r, *b, *stat;
  char bfile[250];
  char sfile[250];
  ssize_t read;
  size_t len = 0;
  char *line = NULL;
  REGION a;
  char lname[MAX_PATH];
  VALUE* Values = malloc(sizeof(VALUE) * total);



  chdir(workdir);

  if (exists(RFILE, 1)) {
    printf("Already merged ... \n");
    return 0;
  }

  d = opendir("sorted");

  printf("Merging regions ...\n");

  if (d) {
    if ((stat = fopen(RFILE,"w+"))) {
      sprintf(bfile, "%s.body", "test01");
      if ((b = fopen(bfile,"w+"))) {
	while ( (e = readdir(d)) != NULL) {
	  if (strstr(e->d_name, ".chr_sorted")) {
	    char *pchr = strrchr(e->d_name, '.');
	    int l = pchr - e->d_name;
	    strncpy(a.name, e->d_name, l);
	    a.name[l] = '\0';
	    printf(" + %s \n", a.name);
	    sprintf(lname, "sorted/%s", e->d_name);

	    if ( (r = fopen(lname, "r"))) {
	      ULONG pos, curpos = -1;
	      VALUE val;
	      int ind;
	      int num;
	      rcur = (RRECORD *) malloc(sizeof(RRECORD));
	      a.offset = ftell(b);
	      
	      while((read = getline(&line, &len, r)) > 0 ) {
		if ((num = sscanf(line, "%ld %f %d", &pos, &val, &ind) == 3)) {
		  if ( curpos != pos ) {
		    if (curpos != -1) {
		      fwrite(&curpos, sizeof(curpos), 1, b);
		      fwrite(Values, sizeof(VALUE), total, b);
		    }
		    curpos = pos;
		    memset(Values, 0, total * sizeof(VALUE));
		  } 
		  *(Values + ind) = val;
		}
	      }

	      a.size = ftell(b)-a.offset;

	      fprintf(stat, "%s %ld %ld\n", a.name, a.offset, a.size);

	      memcpy(&rcur->data, &a, sizeof(REGION));
	      
	      rcur->next = rroot;
	      rcur->prev = NULL;
	      
	      if (rroot) {
		rroot->prev = rcur;
	      }
	      rroot = rcur;
	      
	      fclose(r);
	      
	    } else {
	      printf("Failed to open %s\n", lname);
	    }
	  }
	}
	fclose(b);
      }
      fclose(stat);
    } else {
      printf("Faild to open %s\n", RFILE);
    }
  }

  if (Values) {
    free(Values);
  }

  return 0;
}

int exists(char *fname, int t) {
  struct dirent *e;
  DIR *d;
  char cmd[200];
  struct stat s;

  int err = stat(fname, &s);

  if (err == -1){
    if (errno == ENOENT) {
    }
  } else {
    if (t == 2) {
      if (S_ISDIR(s.st_mode)) {
	return 1;
      }
    }
    if (t == 1) {
      if (S_ISREG(s.st_mode)) {
	return 1;
      }
    }
  }
  return 0;    
}

int sort_regions (char *workdir,  int total ) {
  struct dirent *e;
  DIR *d;
  char cmd[200];
  chdir(workdir);
  struct stat s;

  int err = stat("sorted", &s);
  if (err == -1){
    if (errno == ENOENT) {
    }
  } else {
    if (S_ISDIR(s.st_mode)) {
      printf(" - regions already sorted \n");
      return 0;
    }
  }

  d = opendir(".");
  printf("Sorting regions ... \n");
  system("mkdir sorted");
  if (d) {
    while ( (e = readdir(d)) != NULL) {
      if (strstr(e->d_name, ".chr")) {
	printf(" - %s\n", e->d_name);
	sprintf(cmd, "sort -g %s > sorted/%s_sorted", e->d_name, e->d_name);
	system(cmd);
      }
    }
    closedir(d);
  }
  return 0;
}

char* get_tmp_folder (void) {
  char *path = getenv("TMPDIR");

  if (!path) {
    path = getenv("TMP");
    if (!path) {
      path = "/tmp";
    }
  }

  char template[1024];
  sprintf(template, "%s/mwXXXXXX", path);
  return  mkdtemp(template);
}

int read_bedgraph (char *workdir, char *fname, int idx, int total) {
  FILE *src;
  FILE *header = NULL;
  FILE *stat;

  char hfile[255];

  ssize_t read;
  size_t len = 0;
  char *line = NULL;
  
  ULONG rStart, rEnd;
  VALUE val;

  int num ;
  ULONG pos;
 
  char region[50];
  char curRegion[50] = "undefined";
  char description[255] = "";
  TRACK a;
  VALUE minV;
  VALUE maxV;
  int vSet = 0;
  char *slash, *dot;
  char sfile[255];

  sprintf(sfile, "%s/%s", workdir, TFILE);
  
  if ((stat = fopen(sfile, "a+"))) {
    if ((src = fopen(fname, "r"))) {
      int ic = 0, dc = 0, vc = 0;
      printf(" - adding bedgraph %d of %d (%s) \n", idx+1, total, fname);

      while((read = getline(&line, &len, src)) > 0 ) {
	ic ++;
	if (*line == '#') {
	} else {
	  dc ++;
	  if ((num = sscanf(line, "%s\t%ld\t%ld\t%f", region, &rStart, &rEnd, &val) == 4)) {
	    vc ++;
	    if (strcmp(curRegion, region) != 0) {
	      //	      printf(" write %s \n", region);
	      strcpy(curRegion, region);
	      sprintf(hfile, "%s/%s.chr", workdir, region);
	      //	    	    printf(" writing to %s\n", hfile);
	      if (header) {
		fclose(header);
	      }
	      
	      if ((header = fopen(hfile, "a+"))) {
		//	      		printf("opened %s \n", hfile);
	      } else {
		printf(" Cannot open %s ", hfile);
		exit(-1);
	      }
	    }
	    
	    if (vSet) {
	      if (minV > val ) {
		minV = val ;
	      }
	      if (maxV < val ) {
		maxV = val ;
	      }
	    } else {
	      vSet = 1;
	      minV = val;
	      maxV = val;
	    }
	    
	    for (pos = rStart; pos <=rEnd; pos ++) {
	      fprintf(header, "%ld %f %d\n", pos, val, idx);
	    }
	  }
	}
      }
      fclose(header);
    
      fclose(src);

      //      printf("vc = %d / dc = %d / ic = %d \n", vc, dc, ic);

      tcur = (TRECORD *) malloc(sizeof(TRECORD));
      slash = strrchr(fname, '/');
      
      if (slash) {
	sprintf(a.desc, "%s", slash+1);
      } else {
	sprintf(a.desc, "%s", fname);
      }
      dot = strrchr(a.desc, '.');
      if (dot) {
	strncpy(a.name, a.desc, (dot - a.desc));
      } else {
	sprintf(a.name, a.desc);
      }
      sprintf(a.desc, description);
      a.id = idx;
      a.min = minV;
      a.max = maxV;
      a.ori = 1;
      memcpy(&tcur->data, &a, sizeof(TRACK));
      fprintf(stat, "%d %f %f %d %s %s\n",
	      a.id, a.min, a.max, a.ori, a.name, a.desc);
      tcur->next = troot;
      tcur->prev = NULL;

      if (troot) {
	troot->prev = tcur;
      }
      troot = tcur;
      

      if (line) {
	free(line);
      }
      if (verbose) {
	printf("\t values range: %f .. %f\n", minV, maxV);
      }
    } else {
      printf("Error : could not open %s\n", fname);
    }  
    fclose(stat);
  } else {
    printf("Error : could not open %s\n", TFILE);
  }
  return 0;
}

int read_wig (char *workdir, char *fname, int idx, int total) {
  FILE *src;
  FILE *header = NULL;

  char hfile[255];

  ssize_t read;
  size_t len = 0;
  char *line = NULL;
  
  int state = 0; // reading the step line
  char strStep[50], strChr[50], strSpan[50];
  int step = 0;
  int num ;
  long pos;
  VALUE val;
  char region[50];
  char curRegion[50] = "undefined";
  char description[255] = "";
  TRACK a;
  VALUE minV;
  VALUE maxV;
  int vSet = 0;
  char *slash, *dot;

  if ((src = fopen(fname, "r"))) {
    printf(" - adding wig track %d of %d (%s) \n", idx+1, total, fname);

    while((read = getline(&line, &len, src)) > 0 ) {
      int f = 1;
      while (f) {
	switch(state) {
	case 0:
	  if ((num = sscanf(line, "%s %s %s", strStep, strChr, strSpan) == 3)) {
	    sscanf(strChr, "chrom=%s", region);
	    	    printf("A: %s  * %s * %s \n ", strChr, region, curRegion);
	    if (strstr(strStep, "fixed") != NULL) {
	      step = 1;
	      state = 1; // fixed step
	    } else {
	      state = 2; // variable step
	    }
	    f = 0;
	    	    printf("B: %s * %s \n ", region, curRegion);
	    if (strcmp(curRegion, region) != 0) {
	      	      	      printf(" write %s \n", region);
	      strcpy(curRegion, region);
	      sprintf(hfile, "%s/%s.chr", workdir, region);
	      if (header) {
		fclose(header);
	      }

	      if ((header = fopen(hfile, "a+"))) {
				printf("opened %s (%d) \n", hfile, state);
	      } else {
		printf(" Cannot open %s ", hfile);
		exit(-1);
	      }
	    }
	  }
	  break;
	case 1:
	  f = 0;
	  break;
	case 2:
	  if ((num = sscanf(line, "%ld %f", &pos, &val) == 2)) {
	    f = 0;
	    if (vSet) {
	      if (minV > val ) {
		minV = val ;
	      }
	      if (maxV < val ) {
		maxV = val ;
	      }
	    } else {
	      vSet = 1;
	      minV = val;
	      maxV = val;
	    }
	    fprintf(header, "%ld %f %d\n", pos, val, idx);
	  } else {
	    state = 0;
	  }
	  break;
	default:
	  f = 0;
	  
	}
      }
    }
    fclose(header);
    fclose(src);

    tcur = (TRECORD *) malloc(sizeof(TRECORD));
    slash = strrchr(fname, '/');

    if (slash) {
      sprintf(a.desc, "%s", slash+1);
    } else {
      sprintf(a.desc, "%s", fname);
    }
    dot = strrchr(a.desc, '.');

    if (dot) {
      memset(a.name, 0, sizeof(a.name));
      strncpy(a.name, a.desc, (dot - a.desc));
    } else {
      sprintf(a.name, a.desc);
    }

    sprintf(a.desc, description);
    a.id = idx;
    a.min = minV;
    a.max = maxV;
    a.ori = 1;
    memcpy(&tcur->data, &a, sizeof(TRACK));
    tcur->next = troot;
    tcur->prev = NULL;

    if (troot) {
      troot->prev = tcur;
    }
    troot = tcur;
  }

  if (line) {
    free(line);
  }
  if (verbose) {
    printf("\t values range: %f .. %f\n", minV, maxV);
  }
  return 0;
}

int get_regions(char *workdir) {
  struct dirent *e;
  DIR *d;
  char cmd[200];

  d = opendir(workdir);
  if (d) {
    while ( (e = readdir(d)) != NULL) {
      if (strstr(e->d_name, ".chr")) {
	printf(" - regions retrieved .. \n");
	closedir(d);
	return 1;
      }
    }
    closedir(d);
  }
  return 0;
}

int mw_create(char *fname, char **argv, int argc) {
  int taxon = 0;
  char *tmp = NULL;
  char *assembly = NULL;
  char *desc = NULL;
  char *wdir = NULL;

  char files[MAX_TRACKS][MAX_PATH];

  int i, num = 0;
  char workdir[MAX_PATH];
  char output[MAX_PATH];

  char *slash = strrchr(fname, '/');

  if (!slash) { // relative path - need to add the cur dir to the fname
    getcwd(workdir, MAX_PATH);
    sprintf(output, "%s/%s", workdir, fname);
  } else {
    sprintf(output, "%s", fname);
  }

  while (*argv) {
    if ((*argv)[0] == '-') {
      switch((*argv)[1]) {
      case 'v':
	argv++;
	verbose = atoi(*argv);
	break;
      case 't':
	argv++;
	tmp = *argv;
	if ((taxon = atoi(tmp)) == 0) {
	  printf("Error: invalid Taxonomy ID %s\n", tmp);
	}	
	break;
      case 'a':
	argv++;
	assembly = *argv;
	break;
      case 'd':
	argv++;
	desc = *argv;
	break;
      case 'w': // use this work dir - possibly to pick up from previous step
	argv++;
	wdir = *argv;
	break;
      default:
	printf("Error: unknown option -%c\n", (*argv)[1]);
	return -1;
      }
      argv++;
    } else {
       strcpy(files[num], *argv++);
       num ++;
    }
  }

  if(wdir) {
    sprintf(workdir, "%s", wdir);
  } else {
    sprintf(workdir, "%s" , get_tmp_folder());
  }

  if (verbose) { 
    printf(" Working in %s\n", workdir);
  }

  if (!taxon || !assembly || !desc) {
    printf("Error: Taxonomy ID, Assembly and Description are required to create a MW file\n");
    return -1;
  }
  

  
  if (! get_regions(workdir)) {
    printf("Reading files ...\n");
    for(i= 0; i < num ; i++) {
      char *ext = strrchr(files[i], '.');
      if (ext) {
	ext++;
	if (strcmp(ext, "wig") == 0) {
	  if (read_wig(workdir, files[i], i, num) < 0) {
	    printf("Error : could not parse WIG in %s\n", files[i]);
	    return -1;
	  }
	} else if (strcmp(ext, "bedgraph") == 0) {
	  if (read_bedgraph(workdir, files[i], i, num) < 0) {
	    printf("Error : could not parse BedGraph in %s\n", files[i]);
	    return -1;
	  }
	} else {
	  printf(" Error: Unknown file format %s\n", files[i]);
	  return -1;
	}
	
      }
    }
  }

  if (verbose) {
    printf("Meta data:\n");
    printf(" - Taxon : %d\n", taxon);
    printf(" - Assembly : %s\n", assembly);
    printf(" - Description: %s\n", desc);
  }

  if (sort_regions(workdir, num) < 0) {
    return -1;
  }
  if (merge_regions(workdir, num) < 0) {
    return -1;
  }
  if ( create_index(workdir, output, taxon, assembly, desc) < 0) {
    return -1;
  }

  printf("%s has been created\n", output);
  return 0;
}

META *mw_stats(char *fname) {
  FILE *f;
  HEADER h;
  META *stats =  malloc(sizeof(META) * META_NUM );
  int i = 0;

  if ( (f = fopen(fname, "r"))) {
    if (fread(&h, sizeof(HEADER), 1, f)) {
      sprintf(stats[i].k, "Version.Major"); sprintf(stats[i++].v, "%d", h.v_major);
      sprintf(stats[i].k, "Version.Minor"); sprintf(stats[i++].v, "%d", h.v_minor);
      sprintf(stats[i].k, "TaxID"); sprintf(stats[i++].v, "%d", h.taxon);
      sprintf(stats[i].k, "Assembly");sprintf(stats[i++].v,"%s", h.assembly);
      sprintf(stats[i].k, "Value.Min");sprintf(stats[i++].v, "%f", h.min);
      sprintf(stats[i].k, "Value.Max");sprintf(stats[i++].v, "%f", h.max);
      sprintf(stats[i].k, "Tracks count");sprintf(stats[i++].v, "%d", h.tracks);
      sprintf(stats[i].k, "Regions count");sprintf(stats[i++].v, "%d", h.regions);
      sprintf(stats[i].k, "Desc");sprintf(stats[i++].v, "%s", h.desc);
      sprintf(stats[i].k, "Format"); strcpy(stats[i++].v,  h.format);
    } else {
      printf("Error: could not read %s\n", fname);
      fclose(f);
      return NULL;
    }
    
    fclose(f);
    return stats;
  }
  printf("Error: could not open %s\n", fname);
  return NULL;
}

TRACK *mw_tracks (char *fname) {
  FILE *f;
  HEADER h;
  TRACK *tracks = NULL;
  TRACK *tmp;

  int i = 0;

  if ( (f = fopen(fname, "r"))) {
    if (fread(&h, sizeof(HEADER), 1, f)) {
      tracks =  malloc(sizeof(TRACK) * (h.tracks + 1));
      for (i = 0; i<h.tracks; i++) {
	if ( !fread((tracks+i), sizeof(TRACK), 1, f)) {
	  printf("Error: could not read from %s\n", fname);
	  fclose(f);
	  free(tracks);
	  return NULL;
	} 
      }
      (tracks+h.tracks)->id = 65535; // to indicate the end of the list

    } else {
      printf("Error: could not read %s\n", fname);
      fclose(f);
      return NULL;
    }
    
    fclose(f);
    return tracks;
  }
  printf("Error: could not open %s\n", fname);
  return tracks;
}

REGION *mw_regions (char *fname) {
  FILE *f;
  HEADER h;
  TRACK t;
  REGION *regions = NULL;
  int i = 0;

  if ( (f = fopen(fname, "r"))) {
    if (fread(&h, sizeof(HEADER), 1, f)) {
      for (i = 0; i<h.tracks; i++) {
	if ( !fread(&t, sizeof(TRACK), 1, f)) {
	  printf("Error: could not read from %s\n", fname);
	  fclose(f);
	  return NULL;
	}
      }
      regions =  malloc(sizeof(REGION) * (h.regions+1) );
      for (i = 0; i<h.regions; i++) {
	if ( !fread((regions+i), sizeof(REGION), 1, f)) {
	  printf("Error: could not read from %s\n", fname);
	  fclose(f);
	  return NULL;
	}
      }
      (regions+h.regions)->size = 0; // to indicate the end of the list

    } else {
      printf("Error: could not read %s\n", fname);
      fclose(f);
      return NULL;
    }
    
    fclose(f);
    return regions;
  }
  printf("Error: could not open %s\n", fname);
  return NULL;
}


RESULT *mw_fetch(char *fname, char *region, char *tracks, int winsize, int *tcount) {
  FILE *f;
  HEADER h;
  REGION r;

  ULONG data_offset;
  ULONG region_offset = -1;
  ULONG region_size = -1;

  char region_name[255] = "", region_start[255];
  ULONG start;
  ULONG end;
  ULONG pos = 0;

  size_t cpos = 0;


  UCHAR tarray[MAX_TRACKS];
  const char s[2] = ",";
  int tnum, active = -1;
  char *token;

  RESULT *res = NULL, *tr;
  int i, j;

  char bfunc = 'p'; // p - pick , s - sum, a - avg ? 

  if (region) {
    char *colon = strchr(region, ':');
    char *hiphen = strchr(region, '-');
    memset(region_name, 0, 255);
    if (colon) {
      strncpy(region_name, region, (colon-region));  
      strncpy(region_start, colon+1, hiphen-colon);
      start = atol(region_start);
      end = atol(hiphen+1);
      if (!start || !end) {
	printf("Error: wrong region format %s\n", region);
	return NULL;
      }
    } else {
      sprintf(region_name, region);
    }
    
    //    printf(" Region %s:%ld-%ld\n", region_name, start, end);
  } else{
    printf("Error: mw_fetch requires a region parameter, e.g. I:1-100\n");
    return NULL;
  }

  // assume all tracks have to be read
  memset(tarray, 1, MAX_TRACKS);  
  if (tracks) {
    // tracks are specified
    if ((token = strtok(tracks, s))) {
      if ((strchr(token, '-') == NULL)) { // - means all tracks
	memset(tarray, 0, MAX_TRACKS);
	tnum = atoi(token);
	tarray[tnum] = 1;
	active = 1;
	while (token != NULL) {
	  token = strtok(NULL, s);
	  if (token) {
	    tnum = atoi(token);
	    tarray[tnum] = 1;
	    active ++;
	  }
	}
      }
    }
  }

  if ( (f = fopen(fname, "r"))) {
    if (fread(&h, sizeof(HEADER), 1, f)) {
      if (h.v_major != VERSION_MAJOR ) {
	printf("Error: version mismatch. File has been created with mwiggle v%d.%d, but the current mwiggle is v%d.%d\n",
	       h.v_major, h.v_minor, VERSION_MAJOR, VERSION_MINOR);
	fclose(f);
	return NULL;
      }
      data_offset = h.offset;
      if (active < 0) { // all tracks 
	active = h.tracks;
      }
      *tcount = active;

      res = (RESULT*) malloc (sizeof (RESULT) * (active + 1));
      j = 0;

      for (i = 0; i<h.tracks; i++) {
	tr = (res + j);
	if (fread(&tr->t, sizeof(TRACK), 1, f)) {
	  if (tarray[i]) {
	    j++;
	  }
	} else {
	  printf("Error: could not read %s\n", fname);
	}
      }

      for (i = 0; i<h.regions; i++) {
	if (fread(&r, sizeof(REGION), 1, f)) {
	  //	  	  printf(" %s : %ld\n", r.name, r.offset);
	  if (strcmp(r.name, region_name) == 0) {
	    region_offset = r.offset + data_offset;
	    region_size = r.size + region_offset;
	  }
	} else {
	  printf("Error: could not read %s\n", fname);
	}
      }

      if (region_offset != -1) {
	VALUE *values = malloc(sizeof(VALUE) * (h.tracks));
	fseek(f, region_offset, SEEK_SET);
	cpos = ftell(f);
	//	printf(" we are at %ld ( total size %ld ) ( pos %ld . start %ld ) \n", cpos, region_size, pos, start);

	while (!feof(f) && (cpos < region_size) && (pos < start)) {
	  if (fread(&pos, sizeof (ULONG) , 1, f)) {
	  }
	  if (fread(values, sizeof (VALUE) , h.tracks, f)) {
	  }
	  cpos = ftell(f);
	}

	//	printf(" we are at %ld ( total size %ld ) ( pos %ld . start %ld ) \n", cpos, region_size, pos, start);

	if (pos >= start) {
	  if (winsize) {
	    float step = (end - start + 1) / (float)winsize;
	    int v = 0;
	    int idx;

	    VALUE *binvalues = malloc( sizeof(VALUE) * winsize * h.tracks);
	    memset(binvalues, 0, sizeof(VALUE) * winsize * h.tracks);
	    while (!feof(f) && (cpos < region_size) && (pos <= end)) {
	      v ++;
	      idx = (pos - start) / step;
	      for (i = 0; i < h.tracks; i++) {
		if (tarray[i]) {
		  VALUE *cv = binvalues + winsize * i + idx;
		  switch (bfunc) {
		  case 'a': // aggregate
		    values[i] += *cv;
		    break;
		  default: // pick
		    if (fabs(values[i]) > fabs(*cv)) {
		      *cv = values[i];
		    }
		  }
		}
	      }
	      if (fread(&pos, sizeof (ULONG) , 1, f)) {
	      }
	      if (fread(values, sizeof (VALUE) , h.tracks, f)) {
	      }
	      cpos = ftell(f);
	    }

	    i = 0;
	    for (j = 0; j < h.tracks; j++) {
	      if (tarray[j]) {
		RESULT* ptr = (res + i);
		ptr->v = (VALUE*) malloc(sizeof(VALUE) * winsize);
		memcpy(ptr->v, (binvalues + j*winsize), winsize * sizeof(VALUE));
		i ++;
	      }
	    }	      	

	    
	    return res;
	  }

	  while (!feof(f) && (cpos < region_size) && (pos <= end)) {
	    printf("%ld", pos);
	    for (i = 0; i < h.tracks; i++) {
	      if (tarray[i]) {
		printf(" %f", values[i]);
	      }
	    }
	    printf("\n");

	    if (fread(&pos, sizeof (ULONG) , 1, f)) {}
	    if (fread(values, sizeof (VALUE) , h.tracks, f)) {}
	    cpos = ftell(f);
	  }
	}

	free(values);
      } else {
	printf("Error: could not find the region %s\n", region_name);
      }
      free(res);
	
    } else {
      printf("Error: could not read %s\n", fname);
    }
    fclose(f);
  } else {
    printf("Error: could not open %s\n", fname);
  }

  return NULL;
}

int mw_dump(char *fname, char *region, char *tracks) {
  FILE *f;
  HEADER h;
  REGION r;
  TRACK trackarray [MAX_TRACKS];

  ULONG data_offset;
  ULONG region_offset = -1;
  ULONG region_end = -1;

  char region_name[255] = "", region_start[255];
  ULONG start = 1;
  ULONG end = -1;
  ULONG pos = 0;

  size_t cpos = 0;


  UCHAR tarray[MAX_TRACKS];
  const char s[2] = ",";
  int tnum, active = -1;
  char *token;


  int i, j;

  FILE *farray[MAX_TRACKS];

  // assume all tracks have to be read
  memset(tarray, 1, MAX_TRACKS);  
  if (tracks) {
    printf(" Dumping tracks %s\n", tracks);
    // tracks are specified
    if ((token = strtok(tracks, s))) {
      if ((strchr(token, '-') == NULL)) { // - means all tracks
	memset(tarray, 0, MAX_TRACKS);
	tnum = atoi(token);
	tarray[tnum] = 1;
	active = 1;
	while (token != NULL) {
	  token = strtok(NULL, s);
	  if (token) {
	    tnum = atoi(token);
	    tarray[tnum] = 1;
	    active ++;
	  }
	}
      }
    }
  }

  memset(region_name, 0, 255);

  if (region) {
    char *colon = strchr(region, ':');
    char *hiphen = strchr(region, '-');

    if (colon) {
      strncpy(region_name, region, (colon-region));  
      strncpy(region_start, colon+1, hiphen-colon);
      start = atol(region_start);
      end = atol(hiphen+1);
      if (!start || !end) {
	printf("Error: wrong region format %s\n", region);
	return -1;
      }
    } else {
      sprintf(region_name, region);
    }
    
    printf(" Region %s:%ld-%ld\n", region_name, start, end);
  }


  if ( (f = fopen(fname, "r"))) {
    if (fread(&h, sizeof(HEADER), 1, f)) {
      data_offset = h.offset;
      if (active < 0) { // all tracks 
	active = h.tracks;
      }

      j = 0;

      for (i = 0; i<h.tracks; i++) {
	if (fread(&trackarray[i], sizeof(TRACK), 1, f)) {
	  if (tarray[i]) {
	    char dname[1024];
	    sprintf(dname, "%s-%d.%s", trackarray[i].name, trackarray[i].ori, h.format);

	    if ((farray[i] = fopen(dname, "w"))) {
	    } else {
	      printf("Error: could not write into %s\n", dname);
	    }
	  }
	} else {
	  printf("Error: could not read %s\n", fname);
	}
      }

      if (region) {
	for (i = 0; i<h.regions; i++) {
	  if (fread(&r, sizeof(REGION), 1, f)) {
	    if (strcmp(r.name, region_name) == 0) {
	      printf(" Found %s. %ld  / %ld\n", r.name, r.offset, r.size);
	      region_offset = r.offset + data_offset;
	      region_end = r.size + region_offset;
	    }
	  } else {
	    printf("Error: could not read %s\n", fname);
	  }
	}

	if (region_offset != -1) {
	  VALUE *values;
	  
	  fseek(f, region_offset, SEEK_SET);
	  cpos = ftell(f);

	  values = malloc(sizeof(VALUE) * (h.tracks));

	  while (!feof(f) && (cpos < region_end) && (pos < start)) {
	    if (fread(&pos, sizeof (ULONG) , 1, f)) {
	    }
	    if (fread(values, sizeof (VALUE) , h.tracks, f)) {
	    }
	    cpos = ftell(f);
	  }
	  
	  if (pos > start) {
	    for (i = 0; i < h.tracks; i++) {
	      if (tarray[i]) {
		fprintf(farray[i], "variableStep chrom=%s span=1\n", region_name);
	      }
	    }
	    
	    
	    while (!feof(f) && (cpos < region_end) && (pos < end)) {
	      for (i = 0; i < h.tracks; i++) {
		if (tarray[i]) {
		  fprintf(farray[i], "%ld %f\n", pos, values[i]);
		}
	      }
	      if (fread(&pos, sizeof (ULONG) , 1, f)) {}
	      if (fread(values, sizeof (VALUE) , h.tracks, f)) {}
	      cpos = ftell(f);
	    }
	  }

	  free(values);

	} else {
	  printf("Error: could not find the region %s\n", region);
	  return -1;
	}
      } else { // all regions
	VALUE *values;
	ULONG tpos;

	values = malloc(sizeof(VALUE) * (h.tracks));

	region_offset = sizeof(HEADER) + sizeof(TRACK) * h.tracks;
	fseek(f, region_offset, SEEK_SET);
	tpos = ftell(f);
	for (j = 0; j<h.regions; j++) {
	  fseek(f, tpos, SEEK_SET);
	  if (fread(&r, sizeof(REGION), 1, f)) {
	    tpos = ftell(f);
	    //	    printf(" Found %s. %ld  / %ld\n", r.name, r.offset, r.size);
	    region_offset = r.offset + data_offset;
	    region_end = r.size + region_offset;

	    fseek(f, region_offset, SEEK_SET);
	    cpos = ftell(f);

	    while (!feof(f) && (cpos < region_end)) {
	      for (i = 0; i < h.tracks; i++) {
		if (tarray[i]) {
		  fprintf(farray[i], "%ld %f\n", pos, values[i]);
		}
	      }
	      if (fread(&pos, sizeof (ULONG) , 1, f)) {}
	      if (fread(values, sizeof (VALUE) , h.tracks, f)) {}
	      cpos = ftell(f);
	    }


	  } else {
	    printf("Error: could not read %s\n", fname);
	  }
	}
	    free(values);
      
      }


      for (i = 0; i<h.tracks; i++) {
	if (farray[i]) {
	  fclose(farray[i]);
	}
      }
    }
    fclose(f);
  }

  return 0;

}

