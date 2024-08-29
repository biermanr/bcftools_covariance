#!/bin/bash
#SBATCH --job-name=par_loci_covar                # create a short name for your job
#SBATCH --nodes=1                                # node count
#SBATCH --ntasks=1                               # total number of tasks across all nodes
#SBATCH --cpus-per-task=1                        # cpu-cores per task (equal to $SLURM_CPUS_PER_TASK)
#SBATCH --mem=2G                                 # total cpu requested memory
#SBATCH --time=23:30:00                          # total run time limit (HH:MM:SS)
#SBATCH --array=1-261                            # array'd jobs (each job will have $SLURM_ARRAY_TASK_ID)
#SBATCH --output=slurm_outs/slurm-%x-%A_%a.out   # for array job, name slurm-out with job-name (%x), job-id (%A), and array-num (%a)

#set bash strict mode (http://redsymbol.net/articles/unofficial-bash-strict-mode/)
set -euo pipefail

#need to tell bcftools where to find BCFtools plugins
#also this is the large 98GB .vcf.gz with 7,627 dogs and 29M loci
export BCFTOOLS_PLUGINS=??? #TODO ADD path here
VCF="" #TODO ADD .vcf.gz path here

#get the region from the regions_120k.txt file
REGION=`head -n $SLURM_ARRAY_TASK_ID regions_120k.txt | tail -n 1`

#Subset the large VCF file by number of dogs/regions and pipe to `+covariance_akey`
bcftools view --regions $REGION $VCF | \
    bcftools +covariance_akey

