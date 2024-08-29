#!/bin/bash
#SBATCH --job-name=regions                       # create a short name for your job
#SBATCH --nodes=1                                # node count
#SBATCH --ntasks=1                               # total number of tasks across all nodes
#SBATCH --cpus-per-task=1                        # cpu-cores per task (equal to $SLURM_CPUS_PER_TASK)
#SBATCH --mem=2G                                 # total cpu requested memory
#SBATCH --time=11:00:00                          # total run time limit (HH:MM:SS)
#SBATCH --output=slurm_outs/slurm-%x-%j.out      # for non-array job, name slurm-out with job-name (%x) and job-num (%j)

#set bash strict mode (http://redsymbol.net/articles/unofficial-bash-strict-mode/)
set -euo pipefail

#if you don't have SLURM, run this script with `sh create_regions.sbatch`
#does not use very much RAM (<<1 GB), but could need a lot of time depending
#on how large the .vcf.gz is

NLOCI=120000

VCF="" #NOTE, put path of .vcf.gz here!

zcat $VCF | \
    awk -v nloci=$NLOCI 'BEGIN {
        c="chr1";
        s=0;
        n=0;
    }
    $1 !~ "^#" {
        n+=1;
        if($1 != c){
            print c":"s"-" "n;
            c=$1;
            s=$2;
            n=0;
        }
        if(n == nloci){
            print c":"s"-"$2-1" "n;
            c=$1;
            s=$2;
            n=0;
        }
    }
    END {
        print c":"s"-"$2" "n;
    }' > regions_120k.txt

