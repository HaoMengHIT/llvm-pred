; ModuleID = 'test.bc'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.ompi_predefined_communicator_t = type opaque
%struct.ompi_predefined_datatype_t = type opaque
%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct.ompi_status_public_t = type { i32, i32, i32, i32, i64 }
%struct.ompi_communicator_t = type opaque
%struct.ompi_datatype_t = type opaque

@ompi_mpi_comm_world = external global %struct.ompi_predefined_communicator_t
@ompi_mpi_int = external global %struct.ompi_predefined_datatype_t
@stderr = external global %struct._IO_FILE*
@.str = private unnamed_addr constant [38 x i8] c"myrank = %d, before recv. Bound = %d\0A\00", align 1
@.str1 = private unnamed_addr constant [37 x i8] c"myrank = %d, after recv. Bound = %d\0A\00", align 1
@.str2 = private unnamed_addr constant [7 x i8] c"Over!\0A\00", align 1

; Function Attrs: nounwind uwtable
define i32 @main(i32 %argc, i8** %argv) #0 {
for.end:
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %myrank = alloca i32, align 4
  %a = alloca [1000 x i32], align 16
  %0 = bitcast [1000 x i32]* %a to i8*
  %Bound = alloca i32, align 4
  %status = alloca %struct.ompi_status_public_t, align 8
  store i32 %argc, i32* %argc.addr, align 4, !tbaa !1
  store i8** %argv, i8*** %argv.addr, align 8, !tbaa !5
  call void @llvm.lifetime.start(i64 4000, i8* %0) #1
  store i32 0, i32* %Bound, align 4, !tbaa !1
  %call = call i32 @MPI_Init(i32* %argc.addr, i8*** %argv.addr) #1
  call void @llvm.memset.p0i8.i64(i8* %0, i8 -1, i64 4000, i32 16, i1 false)
  %call1 = call i32 @MPI_Comm_rank(%struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*), i32* %myrank) #1
  %1 = load i32* %myrank, align 4, !tbaa !1
  %cmp2 = icmp eq i32 %1, 0
  br i1 %cmp2, label %for.body5, label %if.else

for.body5:                                        ; preds = %for.body5, %for.end
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body5 ], [ 1, %for.end ]
  %2 = trunc i64 %indvars.iv to i32
  %mul = mul nsw i32 %2, %2
  %3 = add nsw i64 %indvars.iv, -1
  %arrayidx7 = getelementptr inbounds [1000 x i32]* %a, i64 0, i64 %3
  %4 = load i32* %arrayidx7, align 4, !tbaa !1
  %add = add nsw i32 %4, %mul
  %arrayidx9 = getelementptr inbounds [1000 x i32]* %a, i64 0, i64 %indvars.iv
  store i32 %add, i32* %arrayidx9, align 4, !tbaa !1
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, 1000
  br i1 %exitcond, label %for.end14, label %for.body5

for.end14:                                        ; preds = %for.body5
  store i32 %add, i32* %Bound, align 4, !tbaa !1
  %5 = bitcast i32* %Bound to i8*
  %call15 = call i32 @MPI_Send(i8* %5, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @ompi_mpi_int to %struct.ompi_datatype_t*), i32 1, i32 99, %struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*)) #1
  br label %if.end

if.else:                                          ; preds = %for.end
  %6 = load %struct._IO_FILE** @stderr, align 8, !tbaa !5
  %7 = load i32* %Bound, align 4, !tbaa !1
  %call16 = call i32 (%struct._IO_FILE*, i8*, ...)* @fprintf(%struct._IO_FILE* %6, i8* getelementptr inbounds ([38 x i8]* @.str, i64 0, i64 0), i32 %1, i32 %7) #4
  %8 = bitcast i32* %Bound to i8*
  %call17 = call i32 @MPI_Recv(i8* %8, i32 1, %struct.ompi_datatype_t* bitcast (%struct.ompi_predefined_datatype_t* @ompi_mpi_int to %struct.ompi_datatype_t*), i32 0, i32 99, %struct.ompi_communicator_t* bitcast (%struct.ompi_predefined_communicator_t* @ompi_mpi_comm_world to %struct.ompi_communicator_t*), %struct.ompi_status_public_t* %status) #1
  %9 = load %struct._IO_FILE** @stderr, align 8, !tbaa !5
  %10 = load i32* %myrank, align 4, !tbaa !1
  %11 = load i32* %Bound, align 4, !tbaa !1
  %call18 = call i32 (%struct._IO_FILE*, i8*, ...)* @fprintf(%struct._IO_FILE* %9, i8* getelementptr inbounds ([37 x i8]* @.str1, i64 0, i64 0), i32 %10, i32 %11) #4
  %12 = load i32* %Bound, align 4, !tbaa !1
  %rem44 = srem i32 %12, 1000
  %cmp2045 = icmp sgt i32 %rem44, 0
  br i1 %cmp2045, label %for.body21.lr.ph, label %for.end27

for.body21.lr.ph:                                 ; preds = %if.else
  %13 = load i32* %Bound, align 4, !tbaa !1
  %rem = srem i32 %13, 1000
  br label %for.body21

for.body21:                                       ; preds = %for.body21, %for.body21.lr.ph
  %indvars.iv49 = phi i64 [ 0, %for.body21.lr.ph ], [ %indvars.iv.next50, %for.body21 ]
  %14 = trunc i64 %indvars.iv49 to i32
  %mul22 = mul nsw i32 %14, %14
  %arrayidx24 = getelementptr inbounds [1000 x i32]* %a, i64 0, i64 %indvars.iv49
  store i32 %mul22, i32* %arrayidx24, align 4, !tbaa !1
  %indvars.iv.next50 = add nuw nsw i64 %indvars.iv49, 1
  %15 = trunc i64 %indvars.iv.next50 to i32
  %cmp20 = icmp slt i32 %15, %rem
  br i1 %cmp20, label %for.body21, label %for.end27

for.end27:                                        ; preds = %for.body21, %if.else
  %16 = load %struct._IO_FILE** @stderr, align 8, !tbaa !5
  %17 = call i64 @fwrite(i8* getelementptr inbounds ([7 x i8]* @.str2, i64 0, i64 0), i64 6, i64 1, %struct._IO_FILE* %16) #5
  br label %if.end

if.end:                                           ; preds = %for.end27, %for.end14
  %call29 = call i32 @MPI_Finalize() #1
  call void @llvm.lifetime.end(i64 4000, i8* %0) #1
  ret i32 0
}

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

declare i32 @MPI_Init(i32*, i8***) #2

declare i32 @MPI_Comm_rank(%struct.ompi_communicator_t*, i32*) #2

declare i32 @MPI_Send(i8*, i32, %struct.ompi_datatype_t*, i32, i32, %struct.ompi_communicator_t*) #2

; Function Attrs: nounwind
declare i32 @fprintf(%struct._IO_FILE* nocapture, i8* nocapture readonly, ...) #3

declare i32 @MPI_Recv(i8*, i32, %struct.ompi_datatype_t*, i32, i32, %struct.ompi_communicator_t*, %struct.ompi_status_public_t*) #2

declare i32 @MPI_Finalize() #2

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

; Function Attrs: nounwind
declare i64 @fwrite(i8* nocapture, i64, i64, %struct._IO_FILE* nocapture) #1

; Function Attrs: nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture, i8, i64, i32, i1) #1

attributes #0 = { nounwind uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
attributes #2 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { cold nounwind }
attributes #5 = { cold }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.5.0 (https://github.com/clang-omp/clang a5dbd16db2515a5b2fa82c7dd416d370968646b1) (https://github.com/clang-omp/llvm 1c313aa94183e765c450be6bda3913e22abc3073)"}
!1 = metadata !{metadata !2, metadata !2, i64 0}
!2 = metadata !{metadata !"int", metadata !3, i64 0}
!3 = metadata !{metadata !"omnipotent char", metadata !4, i64 0}
!4 = metadata !{metadata !"Simple C/C++ TBAA"}
!5 = metadata !{metadata !6, metadata !6, i64 0}
!6 = metadata !{metadata !"any pointer", metadata !3, i64 0}
