from csv import DictReader


# POSTICIPARE LA PARTENZA

with open("AMT/stop_times.txt", 'r') as csv:
    with open("stop_fixed.csv", 'w') as csv_dest:
        csv_dest.write("trip_id,arrival_time,departure_time,stop_id,stop_sequence,pickup_type,drop_off_type\n")

        input = DictReader(csv, delimiter=",")

        for row in input:

            h, m, s = map(int, row["departure_time"].split(':'))

            s += 20
            if s > 59:
                m += 1
                if m > 59:
                    h += 1

                s %= 60
                m %= 60

            print(f"{row['departure_time']} -> {h:02}:{m:02}:{s:02}")

            row['departure_time'] = f"{h:02}:{m:02}:{s:02}"
            new_row = ",".join(row.values())
            csv_dest.write(new_row + "\n")

