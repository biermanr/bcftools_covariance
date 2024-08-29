Started Aug 22nd 2024; Rob

----------
Motivation
----------
The lab is working on DAP, trying to generate a genotype covariance
matrix similar to the 1000 genomes paper https://www.nature.com/articles/nature15393
"Extended Data Figure 6: Allelic sharing"
* https://www.nature.com/articles/nature15393/figures/10

Here's the repo for the genotype covariance plugin that the lab found.
* https://github.com/mcshane/bcftools_plugins


the lab has already written code that uses this plugin successfully,
but it's too slow trying to do all 7,000 dogs and 29,089,701 loci (29M).

Here's the VCF file provided by collaborators and copied to Della (it's 98 GB)
* /projects/AKEY/akey_vol2/DAP_bams_nobackup/DogAgingProject_2023_N-7627_canfam4.vcf.gz

This VCF file was created by GLIMPSE imputation apparently.
It has 7,627 dogs and has 29M loci

Here's how I counted the loci, it took 148 minutes to run (2.5 hr):
* time zcat /projects/AKEY/akey_vol2/DAP_bams_nobackup/DogAgingProject_2023_N-7627_canfam4.vcf.gz | grep -c -v "^#"


-----------------------------------------
Timing different numbers of dogs and loci
-----------------------------------------
The lab said it took 7 hours to run for 1,000 loci on all 7,627 dogs.
I want to try and get a sense for how this changes if you only do, say 1,000 dogs.

I made a slurm script `timing_test.sbatch` which submits an array of jobs with different
numbers of dogs and number of loci.

|-------------------|-------------------|------------|------------|------------|
|  Number of dogs   |   Number of loci  |  Num CPUs  | Num GB mem |  Duration  |
|-------------------|-------------------|------------|------------|------------|
|          2        |          100      |      1     |     20     |  00:00:02  |
|          2        |          500      |      1     |     20     |  00:00:03  |
|          2        |        1,000      |      1     |     20     |  00:00:04  |
|         10        |          100      |      1     |     20     |  00:00:01  |
|         10        |          500      |      1     |     20     |  00:00:02  |
|         10        |        1,000      |      1     |     20     |  00:00:04  |
|        100        |          100      |      1     |     20     |  00:00:02  |
|        100        |          500      |      1     |     20     |  00:00:03  |
|        100        |        1,000      |      1     |     20     |  00:00:05  |
|        500        |          100      |      1     |     20     |  00:00:01  |
|        500        |          500      |      1     |     20     |  00:00:05  |
|        500        |        1,000      |      1     |     20     |  00:00:07  |
|      1,000        |          100      |      1     |     20     |  00:00:02  |
|      1,000        |          500      |      1     |     20     |  00:00:09  |
|      1,000        |        1,000      |      1     |     20     |  00:00:14  |
|      7,627        |          100      |      1     |     20     |  00:01:17  |
|      7,627        |          500      |      1     |     20     |  00:05:17  |
|      7,627        |        1,000      |      1     |     20     |  00:09:39  |
|-------------------|-------------------|------------|------------|------------|
(the mem efficiency for all these jobs was 2% or less)

The takeaway for me on these short timings is that the scaling is pretty bad
* 1,000 dogs and 1,000 loci only takes 14 seconds,
* but 7,627 dogs and even just 100 loci takes 1.5 min
* also, for a given number of dogs, the scaling for increased loci looks linear
    --> which is what I expected
* scaling in terms of NUMBERS of dogs scales worse than linear

This matches what the lab saw where all dogs and 1,000 loci took 7 minutes.


I'm going to try to scale up to the full number of loci and have the jobs run
for a while overnight. Also going to try 4 CPUs and hopefully that helps.

Created a new file called `timing_test_all_loci.sbatch` to do this
* Trying 8 different numbers of dogs ranging from 2 to 7,627
* Providing 12G of RAM, 4 CPUs, and 24 hours
* These are an array job with jobid `58418290`

|-------------------|-------------------|------------|------------|------------|
|  Number of dogs   |   Number of loci  |  Num CPUs  | Num GB mem |  Duration  |
|-------------------|-------------------|------------|------------|------------|
|          2        |        ALL        |      4     |     12     |  19:51:13  |
|         10        |        ALL        |      4     |     12     |  21:40:52  |
|        100        |        ALL        |      4     |     12     |  22:44:28  |
|        500        |        ALL        |      4     |     12     |  TIMEOUT   |
|      1,000        |        ALL        |      4     |     12     |  TIMEOUT   |
|      2,000        |        ALL        |      4     |     12     |  TIMEOUT   |
|      3,000        |        ALL        |      4     |     12     |  TIMEOUT   |
|      7,627        |        ALL        |      4     |     12     |  TIMEOUT   |
|-------------------|-------------------|------------|------------|------------|
(the CPU efficiencies by `jobstats` were all 25%, so it was only using 1 CPU)
(also these jobs sipped memory, using 12MB for the 2-dog process)
Updates, after 18 hours the "2 dog" run is still in progress.

I gave the jobs 4 CPUs, but they only seem to be using 1 when I check by jobstats (25% efficiency)
I think this might have to do with piping `bcftools view --samples dog1,dog2 file.vcf | bcftools +covariance`
Since +covariance is "streaming" in data from view, I'm guessing it can't make use of all 4 CPUs
I'm going to try and create an intermediate file and see if that works better
--> Actually according to the docs, multi-threading is only used for
   writing compressed files, so assigning more CPUs won't help

I'm also thinking of trying to convert this gzipped VCF into an uncompressed BCF which could be faster to process(?)
--> Using `bcftools view` to do this in `gzip_vcf_to_gzip_bcf.sbatch`
--> I gave this job 48 hours which still might not be enough (took 22 hours)
--> I wonder how large the uncompressed BCF will be (final size is 3.7TB!!!)

Now I'll try the same jobs as before, but using the *uncompressed BCF*
I also changed it so that intermediate uncompressed BCF files get produced.
For some reason I thought this could be faster, but it probably won't be.
At least I'll get timing info for how fast the `bcftools view` vs. `+covariance`
is. I should go back to piping though, these intermediates are going to be huge.
New submission script is:
    `timing_test_all_loci_BCF.sbatch`
|-------------------|-------------------|------------|------------|------------|
|  Number of dogs   |   Number of loci  |  Num CPUs  | Num GB mem |  Duration  |
|-------------------|-------------------|------------|------------|------------|
|          2        |        ALL        |      1     |      2     |  13:28:07  |
|         10        |        ALL        |      1     |      2     |  09:56:50  |
|        100        |        ALL        |      1     |      2     |  10:22:33  |
|        500        |        ALL        |      1     |      2     |  22:20:05  |
|      1,000        |        ALL        |      1     |      2     |  TIMEOUT   |
|      2,000        |        ALL        |      1     |      2     |  TIMEOUT   |
|      3,000        |        ALL        |      1     |      2     |  TIMEOUT   |
|      7,627        |        ALL        |      1     |      2     |  TIMEOUT   |
|-------------------|-------------------|------------|------------|------------|
(jobid is 58483772, I accidentally gave 4 CPUs and 12GB mem again)
It looks like using the uncompressed BCF results in ~2X speedup for 2, 10, and 100 dogs, and now 500 dogs finished!

Since I did the `bcftools view` and `bcftools +covariance` as separate timed steps, here's the breakdown:
|-------------------|-------------------|------------|---------------|
|  Number of dogs   |   Number of loci  |   `view`   | `+covariance` |
|-------------------|-------------------|------------|---------------|
|          2        |        ALL        |  13:27:39  |    00:00:27   |
|         10        |        ALL        |  09:55:54  |    00:00:55   |
|        100        |        ALL        |  09:55:54  |    00:26:37   |
|        500        |        ALL        |  09:55:22  |    12:24:42   |
|-------------------|-------------------|------------|---------------|

So the `view` doesn't scale at all with number of dogs, which I think makes sense,
although I don't know why the 2-dogs took longer than every other condtion.

More of a concern is the `+covariance` which scales terribly.
Going from 100 to 500 dogs is 5X increase, but a 25X time increase which is O(N^2) and expected
because we need to do all pairwise comparisons between dogs.

I'm going to try the same script, but piping from `view` to `+covariance` to see
if the speeds stay the same. If they do then we can avoid the huge intermeidate files.
I'm just going to run 2, 10, 100, 500, and 1000 dogs but the 1000 dogs would need ~88 hours so should timeout.
Also, truly using 1 CPU and 5G mem this time. jobid is 58511447

|-------------------|-------------------|------------|------------|------------|
|  Number of dogs   |   Number of loci  |  Num CPUs  | Num GB mem |  Duration  |
|-------------------|-------------------|------------|------------|------------|
|          2        |        ALL        |      1     |      2     |  16:45:23  |
|         10        |        ALL        |      1     |      2     |  15:28:46  |
|        100        |        ALL        |      1     |      2     |  20:44:43  |
|        500        |        ALL        |      1     |      2     |  19:26:02  |
|      1,000        |        ALL        |      1     |      2     |  TIMEOUT   |
|-------------------|-------------------|------------|------------|------------|

Since I piped `view` to `+covariance`, I don't have individual timings for both.
These times look ok, but surprisingly the 100 dogs case took twice as long for
piping than it did with the intermediate file above. I'm not sure why. The 500
dogs case still finished around the same time. My takeaway is that piping or not
piping are both fine, so no need to make the large intermediate files.

We're going to try and modify the `+covariance` plugin to only operate on a subset
of the sample pairs, which would make the `view` step unecessary.

----------------------------------------------------
Considering editing the `covariance.c` plugin c code
----------------------------------------------------
Right now it's doing all pairs of comparison:
    https://github.com/mcshane/bcftools_plugins/blob/a990215b4a762d1a9e95259b8adfc/covariance.c

but this is not good when trying to split up the problem in parallel because you end up
doing a lot of duplicate work. For example, Imaging you had A,B,C,D,E,F,G,H,I samples and
you wanted to do batches of size 3.

The goal is to select the fewest amount of 3-sample batches that fills in this symmetrical
matrix (you only have to fill in the upper or lower triangle). Actually the current c code
looks like its doing this work twice, but I'm not going to try and change that right now.

 |ABCDEFGHI
-+---------
A|
B|
C|
D|
E|
F|
G|
H|
I|

So if we selected a batch to be ABC, then that would fill in the grid like so:

 |ABCDEFGHI
-+---------
A|...
B|...
C|...
D|
E|
F|
G|
H|
I|

And next we could do DEF, then GHI


 |ABCDEFGHI
-+---------
A|...
B|...
C|...
D|   ...
E|   ...
F|   ...
G|      ...
H|      ...
I|      ...

But then if we want to get ADE, we're already doing a lot of duplicate work (marked by the 5 x's)

 |ABCDEFGHI
-+---------
A|x....
B|...
C|...
D|.  xx.
E|.  xx.
F|   ...
G|      ...
H|      ...
I|      ...

The duplicate work is occuring because the symmetry of always doing all pairs.
This means that we need more batches to get the same amount of work done.
Instead, it would be nice if we could specify the "source" and "dest" samples separately.
For example, if we could specify (A) as the "source" and (A,B,C) as the "dest"
then we could fill in

 |ABCDEFGHI
-+---------
A|...
B|.
C|.
D|
E|
F|
G|
H|
I|

Next we could do (A) and (D,E,F) as the source/dest

 |ABCDEFGHI
-+---------
A|......
B|.
C|.
D|.
E|.
F|.
G|
H|
I|

And then (A) and (G,H,I) as the source/dest

 |ABCDEFGHI
-+---------
A|.........
B|....
C|..
D|..
E|.
F|.
G|.
H|.
I|.

And then we could do (B) and (B,C,D) as the source/dest.

etc. Basically by specifying the dest and source we can be guaranteed to
avoid duplicate work, which is greatly shrink the number of batches we have to do.
I've been seeing that 3,000 dogs in 500 batch-size requires ~8,000 batches in the symmetric case
but, would only require 36 batches with this source/dest approach (500x500x36 = 3000x3000)

I don't think this will be much work, but I don't know see `c` so we'll see~

I spoke with the lab who thinks this is a good idea, and I'm going to make a "new" plugin called:
* /home/huixinx/software/source/bcftools-1.10.2/plugins/covariance_by_sample.c
* (this is currently a direct copy of covariance.c in the same folder)

which needs to be compiled to a .so with (work.sh in the same folder):
* gcc -shared -fPIC -o covariance_by_sample.so covariance_by_sample.c

and then the lab's bcftools will have access to this plugin so long as the `BCFTOOLS_PLUGIN` ENV variable is set:
* export BCFTOOLS_PLUGINS=/home/huixinx/software/source/bcftools-1.10.2/plugins/

then we can run the following and see that I've changed the help message:
* `/projects/AKEY/akey_vol2/huixinx/Software/bin/bcftools +covariance_by_sample --help`



----------------------------------------------
Using a pruned VCF and only including MAF > 5%
----------------------------------------------
The lab shared a new VCF with me which was generated with LD-pruning and MAF > 5% filtering
--> /projects/AKEY/akey_vol2/huixinx/Projects/04.DAP/Bin/DogAgingProject_2023_N-7627_canfam4.mafP5.LdPruned.vcf.gz

This file is only 11G compared to the original file which is 98G
--> /projects/AKEY/akey_vol2/DAP_bams_nobackup/DogAgingProject_2023_N-7627_canfam4.vcf.gz

It still has 7,627 dogs, but only 0.92M loci

I'm going to run this in the background with different numbers of dogs to see if it can finish
--> Created `timing_test_all_loci_pruned_MAF_filt.sbatch` to do this

This is piping `view` to `covariance+`

|-------------------|-----------------------|------------|------------|------------|
|  Number of dogs   |   Number of loci      |  Num CPUs  | Num GB mem |  Duration  |
|-------------------|-----------------------|------------|------------|------------|
|          2        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|         10        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|        100        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|        500        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|      1,000        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|      2,000        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|      3,000        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|      7,627        | 0.92M LD-prune MAF>5% |      1     |     5      |  ??:??:??  |
|-------------------|-----------------------|------------|------------|------------|

These jobs failed with error:
```
[W::bcf_sr_add_reader] No BGZF EOF marker; file '/projects/AKEY/akey_vol2/huixinx/Projects/04.DAP/Bin/DogAgingProject_2023_N-7627_canfam4.mafP5.LdPruned.vcf.gz' may be truncated         
[E::vcf_parse_format] Number of columns at chr35:27761144 does not match the number of samples (3361 vs 7627)                                                                             
Error: VCF parse error
```

So unfortunately I think the VCF is truncated

----------------------------------------------------------
Returning to understanding the +covariance bcftools plugin
----------------------------------------------------------
I've added a lot more comments to our new plugin, copied from +covariance:
* /home/huixinx/software/source/bcftools-1.10.2/plugins/covariance_by_sample.c

I understand the code much better as well as the covariance calculation.
I added more info to the --help message about the different parts of the equation:

    ```
    [rb3242@della8 plugins]$ /projects/AKEY/akey_vol2/huixinx/Software/bin/bcftools +covariance_by_sample --help                                                                                              

    About: Print genotype covariance matrix for given samples
    Usage: bcftools +covariance [General Options] -- [Plugin Options]
    Options:
       run "bcftools plugin" for a list of common options

    Example:
       bcftools +covariance in.vcf

    Definition:
       C(x,y) = sum_{sites i} (x_i - 2*f_i)*(y_i - 2*f_i) /  (2 * f_i * (1 - f_i))

       Where x_i is the dosage at site i in sample x
       y_i is the dosage at site i in sample y
       and f_i is the allele frequency of site i.
    ```

Where dosage means the number of ALT alleles at position i (possible values of 0, 1, 2)

This has made me realize that C(x,y) depends on all samples, not just x and y
because it calculated `f_i` from all samples. This means that the sub-sampling I
had been doing before has the possibility of changing C(x,y) and is not a good thing
to do. Let me check some of the outputs to see if there are differences (wow, yes there are!):
--> Comparing dogs 100014 vs 100030 for all-loci
--> C(100014, 100030) for   2-dog subset: -3592209.99
--> C(100014, 100030) for  10-dog subset:  -960716.84
--> C(100014, 100030) for 100-dog subset:  -221480.66

So subsetting the dogs is NOT OK to do! Glad that I caught this.

Also, since the equation is very simple it's making me think that we can maybe write
it in python instead of making our own c-plugin for BCFTools. I'm not sure if this would
end up being more work though. We would make a table of allele frequencies for each bi-allelic
locus, and then calculate the gene dosage for each sample at each locus. Neither of these
tables would be overly large. Well actually the 3,000 dogs by 95M loci would be large.
But then we could do vectorized pandas/numpy operations.

Wait, more importantly, it seems clear that we can perform the covariance per chromosome, or
smaller chunks of the genome, and then just add them all up at the end!

One of the first things I tried was all dogs, but only a subset of loci,
and these finished pretty quickly. We did 1,000 loci for all 7,000+ dogs and it took ~10 minutes.
So if we did 60,000 loci it would take ~10 hours because this should scale linearly in time.
Since we have 29e6 loci, we'd need around 500 of these 10 hour jobs to complete, which is a
lot, but not a completely unreasonable amount. Most of these could run in parallel.

Actually we could do 120,000 loci, taking ~20 hours and then we'd only need 250 jobs which
could definitely all be running in parallel!


------------------------------------------------------
Sanity checks that summing Cov(x,y) over loci is valid
------------------------------------------------------
I'll use the 10-dog, ALL-LOCI, intermediate file `intermediates/dogs_10_all_loci.bcf`
to verify that summing C(x,y) over multiple genomic regions gives the same result
as running once over all loci as I've been doing.

For this test I'll just split the loci by chromosome and run 38 jobs.
I'm also going to bgzip and index the .bcf so it can randomly access the locations.

I'm going to pipe `view` to `+covariance` to subset the BCF. It will be like:
```
bcftools view \
    --regions "chr1" \
    intermediates/dogs_10_all_loci.bcf.gz | \
    bcftools +covariance
```

in the real attempt I'll use --regions-file instead of --regions


I've made a script to do this "timing_test_by_chrom.sbatch"
which was submitted as an array job of 38 under jobid 58568479.
All the jobs are finishing in under a minute.

I made a one-off pandas jupyter script to sum the elements of all 38 matrices and got this result:

       |-------------|-------------|------------|------------|------------|------------|-------------|------------|----------- |------------|
       |     100014  |     100030  |    100033  |    100035  |    100044  |    100048  |     100069  |    100087  |     10008  |    100101  |
|------|-------------|-------------|------------|------------|------------|------------|-------------|------------|----------- |------------|
|100014|  9042026.61 |  -960716.85 | -912776.60 | -694965.23 | -940415.07 | -654864.27 | -1712665.87 | -923644.65 | -724119.92 | -1517858.14|
|100030|  -960716.85 |  9481242.16 | -958257.24 | -714879.04 | -717052.56 | -849476.22 | -1827777.54 | -939936.10 | -833551.76 | -1679594.86|
|100033|  -912776.60 |  -958257.24 | 9406817.70 | -793107.06 | -888703.47 | -961289.87 | -1662379.10 | -973937.92 | -722126.17 | -1534240.26|
|100035|  -694965.23 |  -714879.04 | -793107.06 | 7910266.47 | -484634.75 | -813113.30 | -1575201.38 | -775445.52 | -555590.99 | -1503329.19|
|100044|  -940415.07 |  -717052.56 | -888703.47 | -484634.75 | 8805326.63 | -751062.99 | -1833708.07 | -826767.18 | -628723.54 | -1734259.00|
|100048|  -654864.27 |  -849476.22 | -961289.87 | -813113.30 | -751062.99 | 9026058.99 | -1740075.86 | -909613.84 | -742388.27 | -1604174.37|
|100069| -1712665.87 | -1827777.54 |-1662379.10 |-1575201.38 |-1833708.07 |-1740075.86 | 11595279.17 |-1874445.49 |-1717093.12 |  2348067.27|
|100087|  -923644.65 |  -939936.10 | -973937.92 | -775445.52 | -826767.18 | -909613.84 | -1874445.49 | 9334415.10 | -404527.50 | -1706096.91|
| 10008|  -724119.92 |  -833551.76 | -722126.17 | -555590.99 | -628723.54 | -742388.27 | -1717093.12 | -404527.50 | 7916936.28 | -1588814.99|
|100101| -1517858.14 | -1679594.86 |-1534240.26 |-1503329.19 |-1734259.00 |-1604174.37 |  2348067.27 |-1706096.91 |-1588814.99 | 10520300.46|
|------|-------------|-------------|------------|------------|------------|------------|-------------|------------|----------- |------------|

And this matches the results of running on all loci at once!
* /projects/AKEY/akey_vol2/huixinx/Software/bin/bcftools +covariance /projects2/AKEY/akey_vol2/rbierman/bcftools_covariance_dap_parallel/intermediates/dogs_10_all_loci.bcf.gz

All in complete agreement except for one slight rounding error in the bottom right for C(100101, 100101) being 10520300.46 vs 10520300.45.

So this is a great confirmation that adding up the C(x,y) for different regions is ok to do!
(this was pretty clear from the equation, but feels nice to do a validation)

-------------------------------------------
Creating 250 regions to process in parallel
-------------------------------------------
Now that I'm confident that we can calculate the C(x,y) in parallel for different regions and then sum,
we just need to split the SNPs into 250 different regions for parallel processing.

How do I get a list of the SNP locations?
I'm just using AWK to group SNPs into regions that have 120,000 SNPs (except at chromosome changes)

This is `create_regions.sbatch` which outputs to `regions_120k.txt`
I think this will take ~2 hours to run. JobID 58571904. Ended up needing 3.5 hours.

Once I have these regions I can make a SLURM array job to calculate covariance per region

Ok, I made a small mistake with ending chromosomes.
For example at the end of chr1 there's this region:
    chr1:118576843-44

which doesn't make sense, because the 44 is the start of the region on chr2.
I'm "fixing this in post" by manually changing this to `chr1:118576843-` and
the same for the other end-of-chromosome regions.

I summed the number of loci in each region and got the correct number: 29,089,701

I then removed the loci count column from the `regions_120k.txt`

------------------------------------------------------------
Side-project of trying to make covariance calculation faster
------------------------------------------------------------
Just a note before I do the array job run, I think I should be able to get a ~2X speedup
for the +covariance script by changing the double-for loop to just iterate through unique
pairs of samples instead of all pairs. It's currently calculating both C(x,y) and C(y,x)

I tried making these changes to our `+covariance_by_sample` and running on the 10-dog ALL loci file:
* /projects/AKEY/akey_vol2/huixinx/Software/bin/bcftools +covariance_by_sample /projects2/AKEY/akey_vol2/rbierman/bcftools_covariance_dap_parallel/intermediates/dogs_10_all_loci.bcf.gz
* This took 42 seconds

I compared the outputs and they were identical to using the normal `+covariance` plugin
* /projects/AKEY/akey_vol2/huixinx/Software/bin/bcftools +covariance /projects2/AKEY/akey_vol2/rbierman/bcftools_covariance_dap_parallel/intermediates/dogs_10_all_loci.bcf.gz
* This took 49 seconds

So this is not really a big deal for timing, I guess for 10 dogs this is not a bottleneck.
For 7,000+ dogs it will make a bigger difference I believe.

There are memory optimizations I could do as well since the `cov` array doesn't
need to be nsamples X nsamples, but it's fine.

Now the `+covariance_by_sample` is incorrectly named, so I'm renaming it to be
`+covariance_akey` just to be vague I guess, in case the function changes again.


-------------------------------------------------------
Running 250 regions in parallel with `+covariance_akey`
-------------------------------------------------------
Copying the `timing_test_by_chrom.sbatch` into `all_dogs_parallel_loci_cov.sbatch`
and made some slight changes to use the full .vcf.gz and `+covariance_akey`.
There are 261 jobs actually and I gave them 23.5 hours and 2G RAM :fingers-crossed:
Since the jobs are less than 24, the are in the `short` queue which we're allowed to have 400 jobs at once
JobID 58591952

??? RESULTS ???

Job 23 finished in 0.5 hours, which makes sense since this was the end of chr2:84480259- and only included ~10,000 loci
If the scaling is linear then the 120,000 regions are 12X larger, and that's 12x0.5 = 6 hours for the jobs!
That would be great!

Job  34 finished in 41 minutes, it was the end of chr3 with ~12.7K regions
Job 162 finished in 1:16, it was the end of chr19 with 23K regions, so this is ~6X smaller than 120K, looks good still!
Job 103 finished in 3:00, it was the end of chr11 with 51K regions, so this is ~2.5X smaller, so still less than 9 hours!
