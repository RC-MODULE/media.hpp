declare -a arr=("poc01" "poc02" "poc03" "poc04" "poc05" "poc06"\
  "mmco01" "mmco02" "mmco03" "mmco04" "mmco05"\
  "rplr01" "rplr02" "rplr03" "rplr04" "rplr05"\
  "slt01" "slt03" )

for i in "${arr[@]}"
do
  echo "$i"
  ./h264-test h264-tests/$i.264 | grep reflist | diff - h264-tests/$i.reflist
done
