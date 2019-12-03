import json


def process():
    data = {}
    with open('data.json') as f:
        data = json.load(f)
        build_data = []
        with open('build_data') as bench:
            build_data = bench.readlines()[0]
            build_data = build_data.split(' ')

        bench_data = []
        with open('bench_data') as bench:
            bench_data = bench.readlines()[0]
            bench_data = bench_data.split(' ')

        data['data'].append({
            'gitTag': build_data[0][:5],
            'buildId': int(build_data[1]),
            'ops': int(bench_data[0]),
            'success': int(bench_data[1])
        })

    with open('data.json', 'w') as f:
        json.dump(data, f)


if __name__ == '__main__':
    process()
