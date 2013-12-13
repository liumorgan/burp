#include "include.h"

static int load_signature(rs_signature_t **sumset, struct cntr *cntr)
{
	rs_result r;
	rs_job_t *job;
//logp("loadsig %s\n", rpath);

	job = rs_loadsig_begin(sumset);
	if((r=do_rs_run(job,
		NULL, NULL, NULL, NULL, NULL, async_get_fd(), -1, cntr)))
	{
		rs_free_sumset(*sumset);
		return r;
	}
	if((r=rs_build_hash_table(*sumset)))
	{
		return r;
	}
	rs_job_free(job);
//logp("end loadsig\n");
//logp("\n");
	return r;
}

static int load_signature_and_send_delta(BFILE *bfd, FILE *in, unsigned long long *bytes, unsigned long long *sentbytes, struct cntr *cntr, size_t datalen)
{
	rs_job_t *job;
	rs_result r;
	rs_signature_t *sumset=NULL;
	unsigned char checksum[MD5_DIGEST_LENGTH+1];
	rs_filebuf_t *infb=NULL;
	rs_filebuf_t *outfb=NULL;
	rs_buffers_t rsbuf;
	memset(&rsbuf, 0, sizeof(rsbuf));

	if(load_signature(&sumset, cntr)) return -1;

//logp("start delta\n");

	if(!(job=rs_delta_begin(sumset)))
	{
		logp("could not start delta job.\n");
		rs_free_sumset(sumset);
		return RS_IO_ERROR;
	}

	if(!(infb=rs_filebuf_new(bfd,
		in, NULL, -1, ASYNC_BUF_LEN, datalen, cntr))
	  || !(outfb=rs_filebuf_new(NULL, NULL,
		NULL, async_get_fd(), ASYNC_BUF_LEN, -1, cntr)))
	{
		logp("could not rs_filebuf_new for delta\n");
		if(infb) rs_filebuf_free(infb);
		return -1;
	}
//logp("start delta loop\n");

	while(1)
	{
		rs_result delresult;
		delresult=rs_async(job, &rsbuf, infb, outfb);
		if(delresult==RS_DONE)
		{
			r=delresult;
//			logp("delresult done\n");
			break;
		}
		else if(delresult==RS_BLOCKED || delresult==RS_RUNNING)
		{
//			logp("delresult running/blocked: %d\n", delresult);
			// Keep going
		}
		else
		{
			logp("error in rs_async for delta: %d\n", delresult);
			r=delresult;
			break;
		}
		// FIX ME: get it to read stuff (errors, for example) here too.
		if(async_rw(NULL, NULL)) return -1;
	}

	if(r!=RS_DONE)
		logp("delta loop returned: %d\n", r);

//logp("after delta loop: %d\n", r);
//logp("\n");

	if(r==RS_DONE)
	{
		*bytes=infb->bytes;
		*sentbytes=outfb->bytes;
		if(!MD5_Final(checksum, &(infb->md5)))
		{
			logp("MD5_Final() failed\n");
			r=RS_IO_ERROR;
		}
	}
	rs_filebuf_free(infb);
	rs_filebuf_free(outfb);
	rs_job_free(job);
	rs_free_sumset(sumset);

	if(r==RS_DONE && write_endfile(*bytes, checksum)) // finish delta file
			return -1;

	//logp("end of load_sig_send_delta\n");

	return r;
}

static int send_whole_file_w(char cmd, const char *fname, const char *datapth, int quick_read, unsigned long long *bytes, const char *encpassword, struct cntr *cntr, int compression, BFILE *bfd, FILE *fp, const char *extrameta, size_t elen, size_t datalen)
{
	if((compression || encpassword) && cmd!=CMD_EFS_FILE)
		return send_whole_file_gzl(fname, datapth, quick_read,
		  bytes, 
		  encpassword, cntr, compression, bfd, fp, extrameta, elen,
		  datalen);
	else
		return send_whole_filel(cmd, fname, datapth, quick_read, bytes, 
		  cntr, bfd, fp, extrameta, elen,
		  datalen);
}

static int forget_file(struct sbufl *sb, char cmd, struct cntr *cntr)
{
	// Tell the server to forget about this
	// file, otherwise it might get stuck
	// on a select waiting for it to arrive.
	if(async_write_str(CMD_INTERRUPT, sb->path))
		return 0;

	if(cmd==CMD_FILE && sb->datapth)
	{
		rs_signature_t *sumset=NULL;
		// The server will be sending
		// us a signature. Munch it up
		// then carry on.
		if(load_signature(&sumset, cntr))
			return -1;
		else rs_free_sumset(sumset);
	}
	return 0;
}

static int do_backup_phase2_client(struct config *conf, int resume)
{
	int ret=0;
	int quit=0;
	char attribs[MAXSTRING];
	// For efficiency, open Windows files for the VSS data, and do not
	// close them until another time around the loop, when the actual
	// data is read.
	BFILE bfd;
	// Windows VSS headers tell us how much file
	// data to expect.
	size_t datalen=0;
#ifdef HAVE_WIN32
	binit(&bfd, 0, conf);
#endif

	struct sbufl sb;
	struct iobuf *rbuf=NULL;

	init_sbufl(&sb);

	if(!resume)
	{
		// Only do this bit if the server did not tell us to resume.
		if(async_write_str(CMD_GEN, "backupphase2")
		  || async_read_expect(CMD_GEN, "ok"))
			return -1;
	}
	else if(conf->send_client_counters)
	{
		// On resume, the server might update the client with the
 		// counters.
		if(recv_counters(conf))
			return -1;	
	}

	if(!(rbuf=iobuf_alloc())) return -1;

	while(!quit)
	{
		if(async_read(rbuf))
		{
			ret=-1;
			quit++;
		}
		else if(rbuf->buf)
		{
			//logp("now: %c:%s\n", rbuf->cmd, rbuf->buf);
			if(rbuf->cmd==CMD_DATAPTH)
			{
				sb.datapth=rbuf->buf;
				rbuf->buf=NULL;
				continue;
			}
			else if(rbuf->cmd==CMD_ATTRIBS)
			{
				// Ignore the stat data - we will fill it
				// in again. Some time may have passed by now,
				// and it is best to make it as fresh as
				// possible.
				iobuf_free_content(rbuf);
				continue;
			}
			else if(rbuf->cmd==CMD_FILE
			  || rbuf->cmd==CMD_ENC_FILE
			  || rbuf->cmd==CMD_METADATA
			  || rbuf->cmd==CMD_ENC_METADATA
			  || rbuf->cmd==CMD_VSS
			  || rbuf->cmd==CMD_ENC_VSS
			  || rbuf->cmd==CMD_VSS_T
			  || rbuf->cmd==CMD_ENC_VSS_T
			  || rbuf->cmd==CMD_EFS_FILE)
			{
				int forget=0;
				uint64_t winattr=0;
				struct stat statbuf;
				char *extrameta=NULL;
				size_t elen=0;
				unsigned long long bytes=0;
				FILE *fp=NULL;
				int compression=conf->compression;

				sb.path=rbuf->buf;
				rbuf->buf=NULL;

#ifdef HAVE_WIN32
				if(win32_lstat(sb.path, &statbuf, &winattr))
#else
				if(lstat(sb.path, &statbuf))
#endif
				{
					logw(conf->cntr, "Path has vanished: %s", sb.path);
					if(forget_file(&sb,
						rbuf->cmd, conf->cntr))
					{
						ret=-1;
						quit++;
					}
					free_sbufl(&sb);
					continue;
				}

				if(conf->min_file_size
				  && statbuf.st_size<
					(boffset_t)conf->min_file_size
				  && (rbuf->cmd==CMD_FILE
				  || rbuf->cmd==CMD_ENC_FILE
				  || rbuf->cmd==CMD_EFS_FILE))
				{
					logw(conf->cntr, "File size decreased below min_file_size after initial scan: %c:%s", rbuf->cmd, sb.path);
					forget++;
				}
				else if(conf->max_file_size
				  && statbuf.st_size>
					(boffset_t)conf->max_file_size
				  && (rbuf->cmd==CMD_FILE
				  || rbuf->cmd==CMD_ENC_FILE
				  || rbuf->cmd==CMD_EFS_FILE))
				{
					logw(conf->cntr, "File size increased above max_file_size after initial scan: %c:%s", rbuf->cmd, sb.path);
					forget++;
				}

				if(!forget)
				{
					compression=in_exclude_comp(conf->excom,
					  sb.path, conf->compression);
					encode_stat(attribs,
					  &statbuf, winattr, compression);
					if(open_file_for_sendl(
#ifdef HAVE_WIN32
						&bfd, NULL,
#else
						NULL, &fp,
#endif
						sb.path, winattr,
						&datalen, conf))
							forget++;
				}

				if(forget)
				{
					if(forget_file(&sb,
					  rbuf->cmd, conf->cntr))
					{
						ret=-1;
						quit++;
					}
					free_sbufl(&sb);
					continue;
				}

				if(rbuf->cmd==CMD_METADATA
				  || rbuf->cmd==CMD_ENC_METADATA
				  || rbuf->cmd==CMD_VSS
				  || rbuf->cmd==CMD_ENC_VSS
#ifdef HAVE_WIN32
				  || conf->strip_vss
#endif
				  )
				{
					if(get_extrameta(
#ifdef HAVE_WIN32
						&bfd,
#endif
						sb.path,
						&statbuf, &extrameta, &elen,
						winattr, conf,
						&datalen))
					{
						logw(conf->cntr, "Meta data error for %s", sb.path);
						free_sbufl(&sb);
						close_file_for_sendl(&bfd, &fp);
						continue;
					}
					if(extrameta)
					{
#ifdef HAVE_WIN32
						if(conf->strip_vss)
						{
							free(extrameta);
							extrameta=NULL;
							elen=0;
						}
#endif
					}
					else
					{
						logw(conf->cntr, "No meta data after all: %s", sb.path);
						free_sbufl(&sb);
						close_file_for_sendl(&bfd, &fp);
						continue;
					}
				}

				if(rbuf->cmd==CMD_FILE && sb.datapth)
				{
					unsigned long long sentbytes=0;
					// Need to do sig/delta stuff.
					if(async_write_str(CMD_DATAPTH, sb.datapth)
					  || async_write_str(CMD_ATTRIBS, attribs)
					  || async_write_str(CMD_FILE, sb.path)
					  || load_signature_and_send_delta(
						&bfd, fp,
						&bytes, &sentbytes, conf->cntr,
						datalen))
					{
						logp("error in sig/delta for %s (%s)\n", sb.path, sb.datapth);
						ret=-1;
						quit++;
					}
					else
					{
						do_filecounter(conf->cntr, CMD_FILE_CHANGED, 1);
						do_filecounter_bytes(conf->cntr, bytes);
						do_filecounter_sentbytes(conf->cntr, sentbytes);
					}
				}
				else
				{
					//logp("need to send whole file: %s\n",
					//	sb.path);
					// send the whole file.

					if((async_write_str(CMD_ATTRIBS, attribs)
					  || async_write_str(rbuf->cmd, sb.path))
					  || send_whole_file_w(rbuf->cmd, sb.path,
						NULL, 0, &bytes,
						conf->encryption_password,
						conf->cntr, compression,
						&bfd, fp,
						extrameta, elen,
						datalen))
					{
						ret=-1;
						quit++;
					}
					else
					{
						do_filecounter(conf->cntr, rbuf->cmd, 1);
						do_filecounter_bytes(conf->cntr, bytes);
						do_filecounter_sentbytes(conf->cntr, bytes);
					}
				}
#ifdef HAVE_WIN32
				// If using Windows do not close bfd - it needs
				// to stay open to read VSS/file data/VSS.
				// It will get closed either when given a
				// different file path, or when this function
				// exits.
				
				//if(rbuf->cmd!=CMD_VSS
				// && rbuf->cmd!=CMD_ENC_VSS)
				//	close_file_for_sendl(&bfd, NULL);
#else
				close_file_for_sendl(NULL, &fp);
#endif
				free_sbufl(&sb);
				if(extrameta) free(extrameta);
			}
			else if(rbuf->cmd==CMD_WARNING)
			{
				do_filecounter(conf->cntr, rbuf->cmd, 0);
				iobuf_free_content(rbuf);
			}
			else if(rbuf->cmd==CMD_GEN && !strcmp(rbuf->buf, "backupphase2end"))
			{
				if(async_write_str(CMD_GEN, "okbackupphase2end"))
					ret=-1;
				quit++;
			}
			else
			{
				iobuf_log_unexpected(rbuf, __FUNCTION__);
				ret=-1;
				quit++;
			}
		}
	}
#ifdef HAVE_WIN32
	// It is possible for a bfd to still be open.
	close_file_for_sendl(&bfd, NULL);
#endif
	iobuf_free(rbuf);
	return ret;
}

int backup_phase2_client_legacy(struct config *conf, int resume)
{
	int ret=0;

	logp("Phase 2 begin (send file data)\n");

	ret=do_backup_phase2_client(conf, resume);

	print_endcounter(conf->cntr);
	print_filecounters(conf, ACTION_BACKUP);

	if(ret) logp("Error in phase 2\n");
	logp("Phase 2 end (send file data)\n");

	return ret;
}
