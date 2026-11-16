# SOSD 검색

Visual Studio 2022 이상에서 **x64 Native Tools Command Prompt**를 열고
저장소 루트에서 빌드한다.

    cl /utf-8 /std:c++17 /EHsc /O2 main.cpp BinarySearch.cpp BlockIndex.cpp SOSDReader.cpp /Fe:SOSD.exe

압축을 푼 32비트 SOSD 파일을 사용한다. 형식은 8바이트 원소 수와 정렬된
32비트 부호 없는 정수 키다. 전체 실험에는 [SOSD 데이터](https://dataverse.harvard.edu/api/access/datafile/:persistentId?persistentId=doi:10.7910/DVN/JGVF9A/5YTV8K)의
`books_200M_uint32`를 사용한다.

    SOSD.exe find books_200M_uint32 12345 4096

검색 결과는 0부터 세는 첫 번째 위치 또는 `NOT_FOUND`다. 블록 크기를
생략하면 키 4,096개를 한 블록으로 사용한다.

같은 파일과 질의 수로 두 방식을 각각 실행한다.

    SOSD.exe bench books_200M_uint32 binary 1000
    SOSD.exe bench books_200M_uint32 block 4096 1000

마지막 인수는 적중 질의와 미적중 질의 각각의 수다. 프로그램은 고정된
난수 시드로 같은 질의 목록을 만들고, 실제 미적중 여부를 확인한다.
목록 생성과 정답 비교는 검색 시간 측정에서 제외한다. `total_ms`는
인덱스 구축, 적중 검색, 미적중 검색 시간의 합이다. 파일 캐시 조건을
맞추려면 두 명령을 먼저 한 번씩 실행한 후 실행 순서를 번갈아 반복한다.

프로그램은 Windows Job Object로 프로세스의 커밋 메모리를 512,000,000바이트로
제한한다. `peak_process_commit_bytes`는 실행 전체의 최대 커밋 메모리다.
기존 방식인 `SOSD.exe <파일> <블록 크기>`는 한 프로세스에서 두 방법을 실행하므로
한 개의 공통 메모리 피크를 출력한다. 방법별 메모리 비교에는 위의 별도 실행을 사용한다.
