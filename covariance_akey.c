/*  plugins/covariance.c -- XXX

    Copyright (C) 2015 Genome Research Ltd.

    Author: Shane McCarthy <sm15@sanger.ac.uk>
            Commented and slightly modified by Rob Bierman <rbierman@princeton.edu>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.  */

#include <stdio.h>
#include <stdlib.h>
#include <vcf.h>
#include <math.h>
#include <getopt.h>

const char *bcftools_plugin_version = "1.0";

void version(const char **bcftools_version, const char **htslib_version)
{
    *bcftools_version = "1.10.2";
    *htslib_version = "1.10.2";
}

// Short about message
const char *about(void)
{
    return "Prints genotype genotype covariance matrix.\n";
}

// Usage message
const char *usage(void)
{
    return 
        "\n"
        "About: Print genotype covariance matrix \n"
        "Usage: bcftools +covariance [General Options] -- [Plugin Options]\n"
        "Options:\n"
        "   run \"bcftools plugin\" for a list of common options\n"
        "\n"
        "Example:\n"
        "   bcftools +covariance in.vcf\n"
        "\n"
        "Definition:\n"
        "   C(x,y) = sum_{sites i} (x_i - 2*f_i)*(y_i - 2*f_i) /  (2 * f_i * (1 - f_i))\n"
        "\n"
        "   Where x_i is the dosage at site i in sample x\n"
        "   y_i is the dosage at site i in sample y\n"
        "   and f_i is the allele frequency of site i.\n"
        "\n";
}

bcf_hdr_t *in_hdr = NULL;
uint8_t *buf = NULL;
int nbuf = 0;   // NB: number of elements, not bytes
int ntags = 0, nsamples = 0;
double *cov, *dsg;

// Gets called for each VCF record
// C(x,y) = sum_{sites i} (x_i - 2*f_i)*(y_i - 2*f_i) /  (2 * f_i * (1 - f_i))
int site_covariance(bcf1_t *rec)
{

    // Get the genotypes `bcf_get_genotypes` defined in vcf.h
    //      `nret` is the number of genotypes, like 0/0, 0/1, or 1/1
    //      `buf` is a 2D array of samples and allele values
    //      `nbuf` doesn't look to be used anywhere in the code
    int nret = bcf_get_genotypes(in_hdr,rec,(void**)&buf,&nbuf);

    // If no genotypes, return early
    if ( nret<0 ) return -1;

    // divide nret by the number of samples to get the max ploidy (will normally be 2 for humans)
    nret /= rec->n_sample;

    // make a pointer to the sample/allele's at this locus
    int32_t *ptr = (int32_t*) buf;

    // Declare variables
    double ac = 0, an = 0;
    int i, j, k;

    // Fill the "dosage" dsg array with zeros. one element per sample
    for (i=0; i<rec->n_sample; i++) dsg[i]=0;

    // Iterate through each sample counting the number of ALT alleles
    for (i=0; i<rec->n_sample; i++)
    {
        // Iterate through all (usually both) alleles for each sample
        for (k=0; k<nret; k++)
        {
            // If sample k doesn't have any information, break out of inner loop
            // since there will not be another allele
            if ( ptr[k]==bcf_int32_missing || ptr[k]==bcf_int32_vector_end || ptr[k]==bcf_gt_missing ) break;

            // `an` is the total "allele-number" over all samples
            // this is not guarenteed to be 2*n_sample since some alleles maybe be missing
            an++;

            // `bcf_gt_allele` gets the VCF 0-based allele index
            // and according to https://github.com/samtools/htslib/issues/1461
            // it will be -1 if no-call, 0 if REF, and larger if ALT
            // we already know that there is a call I think because of the above
            // if-statement check, so this only happens when there is an ALT
            // so maybe ac is "alt-count" for all samples? 
            // and dsg[i] is the alt count for sample i (0, 1, or 2) for 2-ploidy
            if ( bcf_gt_allele(ptr[k]) ) { dsg[i]++; ac++; }
        }

        // Advance the pointer by max_ploidy (usually 2) to get to the next sample
        ptr += nret;
    }

    // RB edit: moved this check outside the for-loops since not sample specific
    // if there are no allele counts or all alleles are alt, no change to covariance
    // for ANY of the cov(x,y) pairs
    if (ac==0 || ac==an) return 0;

    // Perform the actual covariance calculation
    // RB edit: changed j-loop to avoid calculating both cov(x,y) and cov(y,x)
    // (should be a ~2X speedup for this portion of the code)
    for (i=0; i<rec->n_sample; i++)
    {
        for (j=i; j<rec->n_sample; j++)
        {

            // add the contribution of this locus to the covariance
            // `i*nsamples+j` is getting a 1D index for this 2D data. same idea
            // as `cov[i][j]` basically
            //
            // `ac` and `an` depend on all samples, `dsg[i]` and `dsg[j]` only depend on the i/j sample
            // does this mean that the `cov` between i and j depends on other samples? seems strange
            cov[i*nsamples+j]+=(dsg[i]-2*ac/an)*(dsg[j]-2*ac/an)/(2*ac/an*(1-ac/an));
        }
    }

    // Always return 0, the purpose of this function is to update `cov`
    return 0;
}

// Gets called once at startup
int init(int argc, char **argv, bcf_hdr_t *in, bcf_hdr_t *out)
{
    int c;
    static struct option loptions[] =
    {
        {0,0,0,0}
    };
    while ((c = getopt_long(argc, argv, "?h",loptions,NULL)) >= 0)
    {
        switch (c) 
        {
            case 'h':
            case '?':
            default: fprintf(stderr,"%s", usage()); exit(1); break;
        }
    }

    in_hdr = in;

    nsamples = bcf_hdr_nsamples(in_hdr);
    cov = (double*) calloc(nsamples*nsamples,sizeof(double)); //is cov filled with 0's?
    dsg = (double*) calloc(nsamples,sizeof(double));

    return 1;
}

// Gets called once per VCF record
bcf1_t *process(bcf1_t *rec)
{
    if (rec->n_allele!=2) return NULL; // biallelic sites only
    site_covariance(rec);
    return NULL;
}

// Gets called once at the end
void destroy(void)
{
    int i, j;
    printf("# COV, covariance\n");

    // Print the samples as the header row
    for (i=0; i<nsamples; i++)
        printf("\t%s", bcf_hdr_int2id(in_hdr,BCF_DT_SAMPLE,i));
    printf("\n");

    // Print the actual values
    for (i=0; i<nsamples; i++)
    {
        printf("%s", bcf_hdr_int2id(in_hdr,BCF_DT_SAMPLE,i));
        for (j=0; j<nsamples; j++)
        {
            // This is only printing one triangle of the matrix, which is good
            printf("\t%.10f", cov[i<=j ? i*nsamples+j : j*nsamples+i]);
        }
        printf("\n");
    }
    free(dsg);
    free(cov);
    free(buf);
}


