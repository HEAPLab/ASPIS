import cv2
import numpy as np
import argparse

def jpg_to_dat(jpg_file, dat_file):
    # Load the JPEG image
    img = cv2.imread(jpg_file, cv2.IMREAD_GRAYSCALE)

    # Save the intensity values to a DAT file with space-separated values
    np.savetxt(dat_file, img, fmt='%d')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="This script transforms a .jpg file to a .dat file encoded as a sequence of integers.")
    parser.add_argument("jpg_file")
    parser.add_argument("dat_file")

    args = parser.parse_args()
    # Example usage
    jpg_file = args.jpg_file
    dat_file = args.dat_file
    jpg_to_dat(jpg_file, dat_file)