from sys import argv
from csv import DictReader

if len(argv) != 2:
    print("Usage: python3 timetable.py <file>")

with open(argv[1], 'r') as csv:
    with open("stop_times_fixed.csv", 'w') as csv_out:
        input = DictReader(csv, delimiter=",")

        ids = set()

        new_lines = []

        for row in input:

            ids.add(row["trip_id"])
            h, m, s = map(int, row["departure_time"].split(':'))

            s += 20
            if s > 59:
                m += 1
                if m > 59:
                    h += 1

                s %= 60
                m %= 60

            # print(f"{row['departure_time']} -> {h:02}:{m:02}:{s:02}")
            if row["trip_id"].split("-")[3] == "Semaine":
                row['day'] = "Mon_Fri"
            elif row["trip_id"].split("-")[3] == "Samedi":
                row['day'] = "Sat"
            else:
                row['day'] = "Sun"
                
            row["trip_id"] = row["trip_id"].split("-")[0]
            row['departure_time'] = f"{h:02}:{m:02}:{s:02}"
            row.pop("pickup_type")
            row.pop("drop_off_type")
            new_row = ",".join(row.values())
            new_lines.append(new_row + "\n")

        csv_out.write(str(len(ids)) + "\n")
        # csv_out.write("trip_id,arrival_time,departure_time,stop_id,stop_sequence,day\n")
        csv_out.writelines(new_lines)

