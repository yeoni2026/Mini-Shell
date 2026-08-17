# Mini-Shell
 
C로 구현한 간단한 유닉스 미니 쉘(Unix Mini Shell)이다.
 
## 핵심 기능
 
### 1. 기본 실행 루프
`fgets()`로 한 줄 입력을 받고, `strtok()`으로 공백 기준 토큰화한 뒤 `fork()` + `execvp()` + `waitpid()`로 명령어를 실행하고 다시 프롬프트로 돌아가는 기본 루프 구조다.
 
### 2. 내장 명령어
`cd`(`chdir()`), `pwd`(`getcwd()`), `jobs`는 별도 핸들러 함수로 분리해 구현했다. `exit`은 종료 시 `free_heap()`으로 job 리스트를 정리해야 해서, 별도 함수로 빼지 않고 `main()` 안에서 직접 처리했다. `cd`는 자식 프로세스가 아닌 쉘 프로세스 자신이 직접 `chdir()`을 호출해야 하는데, 이는 fork로 생성된 자식이 종료되면 그 프로세스의 작업 디렉토리 변경 사항도 함께 사라지기 때문이다.
 
### 3. I/O 리다이렉션 (`>`, `<`)
`open()`으로 파일을 열고 `dup2()`로 표준입출력(fd 0, 1)을 그 파일에 연결한 뒤, 원본 fd는 `close()`로 정리한다. 존재하지 않는 파일, 누락된 파일명, 중복 리다이렉션 등 예외 상황을 방어적으로 처리했다.
 
### 4. 파이프 (`|`)
2개 이상의 명령어를 파이프로 연결해 실행할 수 있다. `pipe()`로 명령어 개수만큼 파이프를 생성하고, 각 자식 프로세스가 `dup2()`로 자신에게 필요한 파이프 끝만 연결한 뒤, 사용하지 않는 나머지 파이프 fd는 모두 `close()`로 닫는 방식으로 구현했다.
 
### 5. 백그라운드 실행 (`&`) & Job Control
명령어 끝에 `&`가 있으면 `waitpid()` 호출 없이 바로 다음 프롬프트로 돌아간다. 백그라운드로 실행된 각 프로세스는 PID와 명령어 이름을 담은 노드로 연결 리스트(linked list)를 만들어 추적한다. 매 루프 시작 시 `waitpid(-1, &status, WNOHANG)`으로 종료된 자식이 있는지 논블로킹으로 확인해 해당 노드를 리스트에서 제거하며 좀비 프로세스 없이 회수(reap)한다. `jobs` 명령어로 현재 살아있는 백그라운드 프로세스 목록을 확인할 수 있고, 쉘이 종료될 때는 `free_heap()` 함수로 남아있는 job 리스트 노드들을 모두 해제한다.
 
## 빌드 & 실행
 
```bash
git clone https://github.com/yeoni2026/Mini-Shell.git
cd Mini-Shell
make
./main
```
 
## 트러블슈팅
 
진행하면서 겪었던 버그들.
 
<details>
<summary>1. 파이프가 연결되지 않는 문제 — <code>#define</code> 매크로의 함정</summary>
`ls -l | wc -l` 같은 파이프 명령어가 계속 정상 동작하지 않았다. GDB로 확인해본 결과, `ls -l`의 출력과 `wc -l`의 입력이 파이프로 제대로 연결되지 않고 있었다.
 
원인은 파일 디스크립터 배열 인덱스를 계산하던 매크로였다.
 
```c
#define IDX(i, j) (i * 2 + j)
```
 
`#define`은 순수한 텍스트 치환이기 때문에, `IDX(command_num - 1, 0)`을 호출하면 의도했던 `(command_num - 1) * 2 + 0`이 아니라 `command_num - 1 * 2 + 0`으로 치환되어, 연산자 우선순위 때문에 완전히 다른 값이 계산되고 있었다. 이 때문에 `dup2()`가 엉뚱한(때로는 배열 범위를 벗어난) fd를 대상으로 호출되고 있었다.
 
```c
#define IDX(i, j) ((i) * 2 + (j))  // 매크로 파라미터는 반드시 괄호로 감싸야 한다
```
 
이 사건 이후로 매크로 파라미터에는 예외 없이 괄호를 붙이는 습관이 생겼다.
 
</details>
<details>
<summary>2. 파이프의 파일 디스크립터 관리 원칙</summary>
파이프를 읽는 프로세스가 EOF를 인식하려면 해당 파이프의 쓰기 끝(write end)을 열고 있는 프로세스가 시스템 전체에서 0개가 되어야 한다는 점을 이해하고, 각 자식 프로세스가 `dup2()`로 필요한 파이프 끝을 연결한 직후 자신이 사용하지 않는 나머지 fd(자기 것 포함)를 모두 명시적으로 `close()`하도록 구현했다.
 
```c
if (pid[pid_count] == 0){
    if (command_num != 0) {
        dup2(fd[IDX(command_num - 1, 0)], 0);
        close(fd[IDX(command_num - 1, 0)]);
    }
    dup2(fd[IDX(command_num, 1)], 1);
    close(fd[IDX(command_num, 1)]);
    close(fd[IDX(command_num, 0)]);
 
    execvp(argv[start_point], argv + start_point);
    fprintf(stderr, "Command execution failed\n");
    exit(1);
}
if (command_num != 0) close(fd[IDX(command_num - 1, 0)]);
close(fd[IDX(command_num, 1)]);
```
 
</details>
<details>
<summary>3. Ctrl+D(EOF) 시 발생하는 무한 루프 문제</summary>
프로그램 실행 중 Ctrl+D를 누르면 프롬프트가 비정상적으로 반복 출력되는 현상이 있었다.
 
원인을 추적해보니:
- Ctrl+D로 EOF를 만난 스트림은 내부적으로 EOF 플래그가 설정되고, 이후 `fgets()` 호출은 실제 읽기를 시도하지 않고 즉시 `NULL`을 반환한다.
- `fgets()`가 `NULL`을 반환하면 `command` 버퍼는 갱신되지 않고 이전 루프의 값이 그대로 남는다.
- 터미널로의 표준출력은 보통 줄 버퍼링(line buffering)을 사용하는데, `printf("$ ")`에는 개행 문자가 없어 즉시 화면에 출력되지 않고 버퍼에 쌓인다.
가장 단순하게 관찰된 증상은 `command` 버퍼가 애초에 비어있던 경우(`command[0]`이 `'\0'`인 경우)였다. 이때는 매 루프마다 아무 명령어도 실행되지 않고 `printf("$ ")`만 반복되는데, 개행 문자가 한 번도 안 섞이니 버퍼가 계속 쌓이기만 하다가 내부 버퍼가 가득 찼을 때 `$ $ $ $ ...`로 한꺼번에 쏟아지는 형태로 나타났다.
 
반대로 Ctrl+D를 누르기 직전에, 실행하면 에러가 나는 명령어(존재하지 않는 명령어 등)를 쳐뒀던 경우는 조금 다른 모습이었다. `command`에 그 명령어가 그대로 남아있으니 매 루프마다 `fork()`로 자식 프로세스를 만들어 그 명령어를 실행하는데, `fork()`는 부모의 메모리를 그대로 복사하기 때문에 아직 flush되지 않고 부모 버퍼에 쌓여 있던 `"$ "`들까지 통째로 자식한테 함께 복사된다. 자식은 `execvp()` 실패 후 개행 문자가 포함된 에러 메시지를 출력하는데, 이때 자식이 물려받은 버퍼 내용 전체가 그 순간 화면에 한꺼번에 출력된다.
 
문제는 이 출력이 자식이 물려받은 복사본에서만 일어난다는 점이다. 부모 자신의 버퍼는 전혀 비워지지 않고 그대로 남아있어서, 다음 루프에서 `printf("$ ")`를 또 실행하면 그 버퍼에 `"$ "`가 하나 더 쌓인다. 이 과정이 반복될수록 부모 버퍼에 누적된 `"$ "` 개수가 계속 늘어나고, 매번 그 늘어난 상태를 새로 태어난 자식이 물려받아 화면에 뿌리면서 아래처럼 반복할수록 `$` 개수가 계단식으로 늘어나는 피라미드 형태가 나타났다.
 
```
$ command execution failed
$ command execution failed
$ $ command execution failed
$ $ $ command execution failed
$ $ $ $ command execution failed
```
 
`fgets()`의 반환값이 `NULL`인 경우를 감지해 쉘을 안전하게 종료하도록 처리해 해결했다.
 
```c
if (fgets(command, COMMAND_SIZE, stdin) == NULL) {
    printf("exit\n");
    exit(0);
}
```
 
</details>
<details>
<summary>4. 개행 문자 없는 긴 입력 처리</summary>
위 수정 직후에는 매 루프마다 무조건 `while ((c = getchar()) != EOF && c != '\n');`을 실행해서 남은 입력을 비우려고 했다. 그런데 이렇게 하니 `ls` 같은 평범한 명령어를 쳐도 다음 프롬프트가 아예 안 뜨고 멈춰버리는, 더 심각한 문제가 새로 생겼다. 원인은 금방 찾을 수 있었다 — 입력이 짧아서 개행 문자까지 이미 다 읽힌 정상적인 경우에도 이 while문이 무조건 실행되면서, stdin에 더 이상 비울 게 없는데도 계속 다음 입력을 기다리며 멈춰버리는 것이었다.
 
`strchr()`로 개행 문자가 실제로 버퍼 안에 존재하는지 먼저 확인하고, 있을 때만 제거, 없을 때만(즉 입력이 길어서 잘렸을 때만) 나머지를 비우는 방식으로 바로 고쳤다.
 
```c
char *newline = strchr(command, '\n');
if (newline != NULL) {
    *newline = '\0';
} else {
    int c;
    while ((c = getchar()) != EOF && c != '\n');
}
```
 
</details>
<details>
<summary>5. 백그라운드 실행 시 계속 발생했던 문제 — <code>redirection()</code>의 NULL 역참조</summary>
`sleep 50 &`처럼 긴 백그라운드 작업을 실행할 때마다 매번 이 문제가 발생했다. `jobs`로 확인하면, 50초가 지나지 않았는데도 해당 프로세스가 이미 회수(reap)되어 사라져 있었다. GDB watchpoint로 job 리스트가 바뀌는 정확한 시점을 추적한 결과, 진짜로 그 프로세스가 몇 초 만에 비정상 종료되고 있다는 것을 확인했다.
 
원인은 `&` 처리와 리다이렉션 처리가 서로 독립적인 `if`문으로 분리되어 있었던 데 있었다.
 
```c
if (strcmp(argv[i - 1], "&") == 0){
    argv[i - 1] = NULL;
}
if (command_num) {
    ...
}
else redirection(i, argv + start_point);
```
 
`&`가 `NULL`로 지워진 뒤에도, 파이프가 없는 경우(`command_num == 0`)엔 `redirection()`이 항상 호출된다. 이때 `redirection()`에 넘기는 인자 개수(`argc`)는 `&`가 지워지기 전의 원래 토큰 개수(`i`) 그대로였고, 함수 내부 반복문이 이미 `NULL`로 지워진 위치까지 순회하면서 `strcmp(NULL, ">")`를 호출해 정의되지 않은 동작(undefined behavior)을 일으키고 있었다.
 
프로젝트 스펙상 파이프·리다이렉션·백그라운드 실행은 서로 배타적인 경우로 설계했기 때문에, `if`를 `else if`로 바꿔 세 로직이 겹치지 않도록 해 해결했다.
 
```c
if (strcmp(argv[i - 1], "&") == 0){
    argv[i - 1] = NULL;
}
else if (command_num) {
    ...
}
else redirection(i, argv + start_point);
```
 
</details>
<details>
<summary>6. 리다이렉션된 표준출력에서의 에러 메시지 — <code>stderr</code>로 전환</summary>
초기 구현에서는 `dup2()`로 표준출력이 파일에 연결된 상태에서 `printf()`로 에러 메시지를 출력하면, 그 메시지 자체가 화면이 아닌 파일로 들어가버리는 문제가 있었다. 이를 우회하기 위해 에러 문구를 `message` 버퍼에 임시 저장해두었다가, 원래 표준출력(`save_out`)을 `dup2()`로 복구한 뒤 출력하는 방식으로 처리했었다.
 
이후 표준에러(`stderr`, fd 2)는 리다이렉션 로직에서 전혀 건드리지 않는다는 점을 활용해, 모든 에러 출력을 `fprintf(stderr, ...)`로 바꾸는 것으로 문제를 훨씬 근본적으로 해결했다. 표준출력이 파일이든 파이프든 상관없이 에러 메시지는 항상 터미널로 출력되며, 그 결과 `message` 버퍼와 `save_out` 복구 로직 전체가 불필요해져 코드가 크게 단순해졌다.
 
</details>