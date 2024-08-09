import statistics
import sys

def calculate_stats(filename):
    numbers = []

    with open(filename, 'r') as file:
        i = 0
        for line in file:
            try:
                number = float(line.strip())
                numbers.append(number)
            except:
                print("Invalid number found (Line " + str(i) + ").")
            i+=1

    if not numbers:
        print("No valid numbers found in the file! Aborting.")
        return
    
    mean = statistics.mean(numbers)
    median = statistics.median(numbers)
    stdev = statistics.stdev(numbers)
    mininum = min(numbers)
    maximum = max(numbers)

    print("Statistics:")
    print("Mean:", mean)
    print("Median:", median)
    print("Standard deviation:", stdev)
    print("Min:", mininum)
    print("Max:", maximum)
    

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python analysis.py filename")
        sys.exit(1)

    filename = sys.argv[1]
    calculate_stats(filename)