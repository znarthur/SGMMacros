# @(#)show.awk	6.1  07/29/12 CSS
#
# This awk script is mainly used with the "showscans" utility
# included in the spec package.  It creates plots with twelve scans
# per page from spec data files.
#
# In general this awk script can be used in a C-PLOT command file
# such as the following (assuming this awk script is called "show.awk")
# to make summary plots of scans from a spec data file.
#
#  re
#  ft 2
#  sy L
#  eb 0
#  u (echo "ARGS $1 $2 $3 $4";cat $1;echo "#S -1")|awk -f show.awk >show.tmp
#  do show.tmp
#  u rm show.tmp
#
# Such a command file would be invoked with 1 to 4 arguments.  Only the
# first is essential and is the name of the data file.  The second and
# third are the starting and maximum scan number.  The fourth argument
# is the name of the C-PLOT graphics filter.  If the scan number
# arguments are missing, all the scans will be processed.  If the filter
# argument is missing, the current (or default) filter will be used.
#
# The font and symbol are set in the above command file rather than the
# command file produced by the following awk script to make it easy for
# the user to modify those two things.  Error-bar mode is also selected
# in the above command file in order to give you the choice of plotting
# error bars on the tiny plots.
#
# This script should recognize all the standard scan headings produced by
# the standard "spec" macros and uses that information to select the x-axis
# label and the column for the independent variable in the data file.
# The title of each of the small plots is the starting Q vector of the
# displayed scan, taken from the "#Q" line of the "spec" data files.
#
# The script normalizes the data by default, unless the normalization
# produces unplottable data.
#
# Thanks to Ben Ocko at NSLS and Doug Abernathy at MIT for putting
# the first versions of this script together under the name "fig".
#

BEGIN {
	normalize_scan = 1;  normalize_none = 0
	start[0] = 1; finish[0] = 99999
	xmin = ymin = ymin_m = big = 100000000
	xmax = ymax = ymax_m = -big
}
# Arguments from command file
/^ARGS/ {
	if (NF < 2) {
		forget_it = 1
		print "The awk script requires a file name!" >"/dev/tty"
		exit
	}
	filename = $2
	NARGS = 1
	if (NF > 2) start[0]  = $3
	if (NF > 3) finish[0] = $4
	filter = ""
	if (NF > 4) filter = $5
	printf("tu 1\n")
	printf("cs\n2.5\n3\n3\n3\n4\n3\n")
	printf("gd 1\n0 0\n1 1\n^D\n")          # For benefit of next "ro"
	printf("ro x\n0\n1\nn\ny\ny\n2\n")
	printf("fn scans.4 +r +S -f %s\n", filename)
	printf("zi %s\n\n", filter)
}
# For global options
/^OPTS/ {
	for (i = 2; i <= NF; i++) {
		if ($i == "-n")
			normalize_none = 1
		if ($i == "-w")
			wait = 1
	}
}
# Alternate invocation uses FILE, START and FINISH
/^FILE/ {
	if (NF < 2) {
		forget_it = 1
		print "The awk script requires a file name!" >"/dev/tty"
		exit
	}
	filename = $2
}
/^START/ {
	start[0] = 1
	for (i = 0; i < NF; i++)
		start[i]=$(i)
}
/^FINISH/ {
	finish[0] = 99999
	for (i = 0; i < NF; i++)
		finish[i] = $(i)
	NARGS = i
	printf("tu 1\n")
	printf("cs\n2.5\n3\n3\n3\n4\n3\n")
	printf("gd 1\n0 0\n1 1\n^D\n")          # For benefit of next "ro"
	printf("ro x\n0\n1\nn\ny\ny\n2\n")
	printf("fn scans.4 +r +S -f %s\n", filename)
}
# A data point
/^[0-9+.\-]/ {
	if (do_this_one) {
		x = $xcol
		y = $NF                 # Number of fields
		monitor = $(NF-1)
		if (x > xmax) xmax = x
		if (x < xmin) xmin = x
		if (y > ymax) ymax = y
		if (y < ymin) ymin = y
		if (!normalize_none && monitor > 0) {
			y_m = y / monitor
			if (y_m > ymax_m) ymax_m = y_m
			if (y_m < ymin_m) ymin_m = y_m
		}
	}
}
# Date of scan
/^#D/ {
	if (do_this_one && n%12 == 0)
		scan_date=sprintf("%s %s, %s", $3, $4, $6)
}
/^#L/ {
	default_xlabel=$2
}
# Scan number and heading
/^#S/ {
	doit = 1
	# if normalized points don't have valid range, use raw data
	if (ymax_m <= ymin_m)
		normalize_scan = 0
	if (!do_this_one)
		doit = 0
	else if (xmax <= xmin)
		doit = 0
	else if (!normalize_scan && ymax <= ymin)
		doit = 0
	if (doit) {
		if (n%12 == 0) {
			printf("zqew\n")
			printf("wi 9\n")
			printf("zn 9 1\n")
			printf("\\C%s  %s\n", filename, scan_date)
			printf("^D\n")
		}
		# The next three lines determine the size of the plots
		x = 2 + 6 * (n%3)
		y = 19.5 - 6 * int((n%12)/3)
		printf("wi %g %g 4 4\n", x, y)
		# (?: operator disappeared from Sun awk)
		if (normalize_scan)
			s = "+n"
		else
			s = "-n"
		printf("fn . %s x=%1.0f %d\n", s, xcol, scan_num)
		printf("tx t %s\n", saveQ)
		# move x label a bit up and make units a bit smaller.
		# don't need to balance motion and size change in this case.
		printf("tx x \\v'-4'%s\\s\n", xlabel)
		printf("zapbltn\n")
		printf("#%d\n", scan_num)
		printf("^D\n")
		if ((++n)%12 == 0) {
			if (wait)
				printf("w\n")
			printf("zx\n")
		}
	}
	do_this_one = 0
	for (i = 0; i < NARGS; i++) if (start[i] <= $2 && $2 <= finish[i]) {
		do_this_one = 1
		normalize_scan = 1
		xmin = ymin = ymin_m = big
		xmax = ymax = ymax_m = -big
		scan_num = $2
		xcol = 1; xlabel = default_xlabel
		default_xlabel = ""
		saveQ = ""
		if ($3 == "ascan") {xcol = 1; xlabel = $4}
		if ($3 == "hkscan" && $4 != $5) {xcol = 1; xlabel = "H"}
		if ($3 == "hkscan" && $6 != $7) {xcol = 2; xlabel = "K"}
		if ($3 == "hklscan" && $4 != $5) {xcol = 1; xlabel = "H"}
		if ($3 == "hklscan" && $6 != $7) {xcol = 2; xlabel = "K"}
		if ($3 == "hklscan" && $8 != $9) {xcol = 3; xlabel = "L"}
		if ($3 == "hkcircle") xlabel = "atan(H/K)"
		if ($3 == "hlcircle") xlabel = "atan(H/L)"
		if ($3 == "klcircle") xlabel = "atan(K/L)"
		if ($3 == "hkradial") xlabel = "sqrt(H*H + K*K)"
		if ($3 == "hlradial") xlabel = "sqrt(H*H + L*L)"
		if ($3 == "klradial") xlabel = "sqrt(K*K + L*L)"
		if ($3 == "Escan")    xlabel = "energy"
		if ($3 == "tscan")    xlabel = "T"
		if ($3 == "abscan" && $4 != $5) {xcol = 1; xlabel = "alpha"}
		if ($3 == "abscan" && $6 != $7) {xcol = 2; xlabel = "beta"}
	}
}
/^#Q/ {
	if (do_this_one)
		saveQ = sprintf("(%.3g,\\|%.3g,\\|%.3g)",$2,$3,$4)
}
END {
	if (!forget_it) {
		if (wait && n%12)
			printf("w\n")
		printf("zs\n")
	}
}
