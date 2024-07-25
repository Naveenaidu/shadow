import requests
from tqdm import tqdm
import filecmp


def fetch_in_chunks(url, chunk_size=100 * 1024 * 1024, output_file='output_file', original_file='original_file'):
    # Send a HEAD request to get the total size of the file
    response = requests.head(url)
    if response.status_code != 200:
        raise Exception(f"Failed to get file size: {response.status_code}")

    file_size = int(response.headers.get('Content-Length', 0))
    print(f"Total file size: {file_size} bytes")

    # Initialize the progress bar
    progress_bar = tqdm(total=file_size, unit='B', unit_scale=True, desc=output_file)

    # Open the output file in binary write mode
    with open(output_file, 'wb') as f:
        for start in range(0, file_size, chunk_size):
            end = min(start + chunk_size - 1, file_size - 1)
            headers = {
                "Range": f"bytes={start}-{end}"
            }
            # print(f"Fetching bytes from {start} to {end}")
            chunk_response = requests.get(url, headers=headers, stream=True)
            if chunk_response.status_code not in [200, 206]:
                raise Exception(f"Failed to fetch chunk: {chunk_response.status_code}")

            # Write the chunk to the file and update the progress bar
            for chunk in chunk_response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
                    progress_bar.update(len(chunk))

    progress_bar.close()
    print(f"File downloaded successfully as {output_file}")

    # Compare the downloaded file with the original file
    if filecmp.cmp(output_file, original_file, shallow=False):
        print("The downloaded file matches the original file.")
    else:
        print("The downloaded file does not match the original file.")

# Example usage
url = 'http://localhost:8082/tmp/upload_test'
fetch_in_chunks(url, output_file='/tmp/upload_test_1', original_file='/tmp/upload_test')